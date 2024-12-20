//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTVisitor.h"

/**
 * To implement a form of garbage collection, reference counting should be applied.
 * The <Object>::allocation array serves as a reference counter for each object.
 * When an object is created with the __init__ method, the reference counter is set to 1.
 * The reference counter is also increased using the __incr__ method when:
 * 1. The pointer is the r_value in an assignment.
 * 2. The pointer is the r_value in a declaration.
 *
 * If the allocation array is <= 0, the object is considered deleted.
 * The reference counter is decreased using the __del__ method when:
 * 1. The pointer is the l_value in an assignment.
 * 2. At the end of the scope for each local pointer of this scope.
 * For each struct, a custom delete method must be automatically created, which in the simplest case only reduces the reference
 * count of the object, and in the case of recursive data structures, also reduces the reference count of all member structs.
 * Cases:
 * 1. The struct has no other objects as members: dec(allocation[self])
 * 2. The struct has other objects as members: dec(allocation[self]) and delete(member)
 * 3. The struct has recursive objects as members: while loop with pseudo recursion
 *
 * Also marks temporary constructors that have to be deleted later on in ReturnFunctionRewriting
 *
 * Implement also:
 * - determine max_individual_structs_var by looking at where the constructors are called:
 * - only determine if constructor is called in the on init callback or on persistence_changed
 * 		- constructor called in linear for loop -> max_individual_structs_var + for loop elements
 */
class ASTPointerScope : public ASTVisitor {
private:
	std::vector<NodeLoop*> m_loop_stack;
	bool is_linear_environment = false;
	std::unordered_map<NodeStruct*, std::unique_ptr<NodeAST>> m_num_constructors;

	DefinitionProvider* m_def_provider = nullptr;
	std::vector<std::unordered_map<StringTypeKey, NodeDataStructure*, StringTypeKeyHash>> m_pointer_scope_stack;

	// which references can be pointers?
	inline static std::unordered_set<NodeType> pointer_types = {
		NodeType::PointerRef,
		NodeType::ArrayRef,
		NodeType::NDArrayRef,
	};

	std::unordered_map<StringTypeKey, NodeDataStructure*, StringTypeKeyHash> remove_scope() {
		if (!m_pointer_scope_stack.empty()) {
			auto scope = std::move(m_pointer_scope_stack.back());
			m_pointer_scope_stack.pop_back();
			return scope;
		}
		return {}; // Leere Map, wenn kein Scope vorhanden ist
	}

	bool remove_pointer(const NodeReference& ref) {
		// if declaration is array and ptr to be deleted is only idx of array, return false
		// we do not handle this case for now
		if(ref.get_declaration() and ref.get_declaration()->ty != ref.ty) {
			return false;
		}

		for (auto it = m_pointer_scope_stack.rbegin(); it != m_pointer_scope_stack.rend(); ++it) {
			auto& current_scope = *it;
			auto pointer_it = current_scope.find({ref.name, ref.ty});
			if (pointer_it != current_scope.end()) {
				current_scope.erase(pointer_it);
				return true; // Erfolgreich entfernt
			}
		}
		return false; // Nicht gefunden
	}


public:
	explicit ASTPointerScope(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	};

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;

		// most func defs will be visited when called, keeping local scopes in mind
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}
//		for(auto & func_def : node.function_definitions) {
//			if(!func_def->visited) {
//				func_def->accept(*this);
//			}
//		}

		node.reset_function_visited_flag();

		for(auto & struct_def : m_num_constructors) {
			struct_def.second->do_constant_folding();
			struct_def.first->max_individual_structs_count = std::move(struct_def.second);
			struct_def.first->max_individual_structs_count->parent = struct_def.first;
		}

		return &node;
	}

	NodeAST* visit(NodeBlock &node) override {
		if(node.scope) m_pointer_scope_stack.emplace_back();
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		// add delete statements for all local pointers
		if(node.scope) {
			auto local_ptrs = remove_scope();
			for(auto & [key, ptr] : local_ptrs) {
				auto ref = ptr->to_reference();
				auto del = std::make_unique<NodeSingleDelete>(std::move(ref), std::make_unique<NodeInt>(1, ptr->tok), ptr->tok);
				node.add_as_stmt(std::move(del));
			}
		}
		node.flatten();
		return &node;
	}

	NodeAST* visit(NodeSingleDelete &node) override {
		// delete statement can only be applied to pointers or arrays -> not to array indices
		if((node.ptr->get_node_type() == NodeType::Array or node.ptr->get_node_type() == NodeType::NDArray)
			and node.ptr->ty->get_type_kind() != TypeKind::Composite) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "Delete statement can only be applied to pointers or arrays, not to array indices.";
			error.exit();
		}
		node.ptr->accept(*this);
		remove_pointer(*node.ptr);
		return &node;
	}

	NodeAST* visit(NodeReturn &node) override {
		for(auto & ret : node.return_variables) {
			ret->accept(*this);
			if(auto ref = cast_node<NodeReference>(ret.get())) {
				remove_pointer(*ref);
			}
		}
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment &node) override {
//		if(node.l_value->is_member_ref()) return &node;

		node.l_value->accept(*this);
		node.r_value->accept(*this);

		if(alters_ref_count(&node)) {
			node.remove_references();
			auto new_assign = std::make_unique<NodeSingleAssignment>(std::move(node.l_value), std::move(node.r_value), node.tok);
			new_assign->collect_references();
			auto new_block = std::make_unique<NodeBlock>(node.tok);
			new_block->add_as_stmt(std::move(new_assign));
			auto assign = new_block->get_last_statement()->cast<NodeSingleAssignment>();
			if(auto del = add_delete(*assign)) {
				new_block->prepend_body(std::move(del));
			}
			if(auto retain = add_retain(assign->l_value.get(), assign->r_value.get(), false)) {
				new_block->append_body(std::move(retain));
			}
			return node.replace_with(std::move(new_block));
		}

		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration &node) override {
		if(node.variable->is_member()) return &node;

		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);

		if(node.variable->is_local and node.variable->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			m_pointer_scope_stack.back()[{node.variable->name, node.variable->ty}] = node.variable.get();
		}

		if(alters_ref_count(&node)) {
			if(auto retain = add_retain(node.variable->to_reference().get(), node.value.get(), true)) {
				retain->prepend_as_stmt(std::make_unique<NodeSingleDeclaration>(node.variable, std::move(node.value), node.tok));
				return node.replace_with(std::move(retain));
			} else {
				node.has_object = true;
			}
		}

		return &node;
	}

	//------- Nodes for struct instance analysis -------

	NodeAST* visit(NodeCallback& node) override {
		if(&node == m_program->init_callback or node.begin_callback == "on persistence_changed") {
			is_linear_environment = true;
		}

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		is_linear_environment = false;
		return &node;
	};

	NodeAST* visit(NodeFor& node) override {
		node.iterator->accept(*this);
		node.iterator_end->accept(*this);
		if(node.step) node.step->accept(*this);

		m_loop_stack.push_back(&node);
		node.body->accept(*this);
		m_loop_stack.pop_back();
		return &node;
	};

	NodeAST* visit(NodeForEach& node) override {
		if(node.key) node.key->accept(*this);
		if(node.value) node.value->accept(*this);
		node.range->accept(*this);

		m_loop_stack.push_back(&node);
		node.body->accept(*this);
		m_loop_stack.pop_back();
		return &node;
	};

	NodeAST* visit(NodeWhile& node) override {
		m_loop_stack.push_back(&node);
		node.condition->accept(*this);
		node.body->accept(*this);
		m_loop_stack.pop_back();
		return &node;
	};

	// if constructor, get struct and increase constructor count
	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		if(node.get_definition() and node.kind != NodeFunctionCall::Kind::Builtin) {
			if(!node.get_definition()->visited) node.get_definition()->accept(*this);
			node.get_definition()->visited = true;
		}

		if(node.kind == NodeFunctionCall::Kind::Constructor) {
			node.is_temporary_constructor = node.is_func_arg() || node.parent->cast<NodeAccessChain>();
			if(auto struct_def = node.get_definition()->parent->cast<NodeStruct>()) {
				increase_num_constructors(struct_def);
			}
		}


		return &node;
	};

	NodeAST* visit(NodeFunctionDefinition& node) override {
		// do this because function definitions are also in the structs
		if(node.visited) return &node;

		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		return &node;
	};

private:

	void add_expr_to_num_constructors(NodeStruct* key, std::unique_ptr<NodeAST> expr) {
		auto it = m_num_constructors.find(key);
		if(it != m_num_constructors.end()) {
			auto& num = it->second;
			auto new_expr = std::make_unique<NodeBinaryExpr>(token::ADD, std::move(expr), std::move(num), expr->tok);
			num = std::move(new_expr);
		} else {
			m_num_constructors[key] = std::move(expr);
		}
	}

	// multiply all loop iterations together if there are multiple loops
	std::unique_ptr<NodeAST> determine_num_iterations() {
		std::unique_ptr<NodeAST> all_num;
		for(auto loop: m_loop_stack) {
			if(loop->determine_linear()) {
				if(auto num = loop->get_num_iterations()) {
					all_num = all_num ? std::make_unique<NodeBinaryExpr>(token::MULT, std::move(all_num), std::move(num), num->tok) : std::move(num);
				}
			} else {
				return nullptr;
			}
		}
		return all_num;
	}

	void increase_num_constructors(NodeStruct* struct_def) {
		if(is_linear_environment) {
			// if the constructor is called in a linear loop environment, add the number of iterations to the constructor count
			if(!m_loop_stack.empty()) {
				if(auto num = determine_num_iterations()) {
					add_expr_to_num_constructors(struct_def, std::move(num));
				}
			} else {
				add_expr_to_num_constructors(struct_def, std::make_unique<NodeInt>(1, Token()));
			}
		}
	}

	// if l_value is of type object and r_value is no constructor
	static bool alters_ref_count(NodeAST* node) {
		Type* l_value_type = nullptr;
		NodeAST* r_value;
		if(auto decl = node->cast<NodeSingleDeclaration>()) {
			l_value_type = decl->variable->ty->get_element_type();
			if(!decl->value) {
				return false;
			}
			r_value = decl->value.get();
		} else if(auto assign = node->cast<NodeSingleAssignment>()) {
			l_value_type = assign->l_value->ty->get_element_type();
			r_value = assign->r_value.get();
		} else {
			return false;
		}

		if(l_value_type->cast<ObjectType>()) {
			if(auto func_call = r_value->cast<NodeFunctionCall>()) {
				// when constructor and declaration -> do not alter ref count
				if(func_call->kind == NodeFunctionCall::Kind::Constructor and node->cast<NodeSingleDeclaration>()) {
					return false;
				}
			}
			if(r_value->is_nil()) {
				return false;
			}
			return true;
		}
		return false;
	}

	static std::unique_ptr<NodeBlock> add_delete(NodeSingleAssignment& assign) {
		auto variable = assign.l_value.get();
		auto value = get_return_var_ptr(assign.r_value.get());
		// add delete before constructor assignment in assign statements only for safety
		if(auto func_call = assign.r_value->cast<NodeFunctionCall>()) {
			if (func_call->kind == NodeFunctionCall::Kind::Constructor) {
				value = func_call;
			}
		}
		if(!value) return nullptr;

		auto del = std::make_unique<NodeBlock>(assign.tok);

		// if r_value is initializer list, be more careful
		auto r_initializer_list = value->cast<NodeInitializerList>();

		// put whole l_value in delete statement -> size and iterations will be figured out at lowering phase
		if(!r_initializer_list) {
			del->add_as_single_delete(clone_as<NodeReference>(variable));
			return del;
		}

		// if initializer is only 1 element -> delete whole array
		// arr[*] := (ls)
		if(r_initializer_list->size() == 1) {
			del->add_as_single_delete(clone_as<NodeReference>(variable));
			return del;
		}

		// if l_value is array and r_value is initializer list with multiple elements
		if(variable->cast<NodeArrayRef>() and r_initializer_list) {
			// arr[*] := (el1, el2, ...)
			for(int i=0; i<r_initializer_list->size(); i++) {
				auto new_array_ref = clone_as<NodeArrayRef>(variable);
				new_array_ref->set_index(std::make_unique<NodeInt>(i, variable->tok));
				new_array_ref->ty = new_array_ref->ty->get_element_type();
				del->add_as_single_delete(std::move(new_array_ref));
			}
			return del;
		}

		// more_lists[4, *] := (ls, ls1) // <-
		if(auto nd_array = variable->cast<NodeNDArrayRef>()) {
			for (int i = 0; i<r_initializer_list->size(); i++) {
				auto new_nd_array_ref = clone_as<NodeNDArrayRef>(nd_array);
				new_nd_array_ref->replace_next_wildcard_with_index(std::make_unique<NodeInt>(i, Token()));
				new_nd_array_ref->ty = new_nd_array_ref->ty->get_element_type();
				del->add_as_single_delete(std::move(new_nd_array_ref));
			}
			return del;
		}

		auto error = CompileError(ErrorType::InternalError, "", "", assign.tok);
		error.m_message = "Invalid delete statement. Could not determine delete statement.";
		error.exit();

		return nullptr;
	}

	/// assuming variable is of type object
	static std::unique_ptr<NodeBlock> add_retain(NodeReference* var, NodeAST* val, bool is_decl = true) {
		auto variable = var;
		auto value = get_return_var_ptr(val);

		if(!value) return nullptr;
		auto retain = std::make_unique<NodeBlock>(variable->tok);

		// if r_value is initializer list, be more careful
		auto r_initializer_list = value->cast<NodeInitializerList>();

		if(!r_initializer_list) {
			retain->add_as_single_retain(clone_as<NodeReference>(variable), std::make_unique<NodeInt>(1, variable->tok));
			return retain;
		}

		auto composite_ref = cast_node<NodeCompositeRef>(variable);
		if(r_initializer_list->size() == 1) {
			retain->add_as_single_retain(clone_as<NodeReference>(variable), std::make_unique<NodeInt>(1, variable->tok));
			return retain;
		}

		// if l_value is array and r_value is initializer list with multiple elements
		if(auto array_ref = variable->cast<NodeArrayRef>()) {
			// arr[*] := (el1, el2, ...)
			for(int i=0; i<r_initializer_list->size(); i++) {
				auto new_array_ref = clone_as<NodeArrayRef>(variable);
				new_array_ref->set_index(std::make_unique<NodeInt>(i, variable->tok));
				new_array_ref->ty = new_array_ref->ty->get_element_type();
				retain->add_as_single_retain(std::move(new_array_ref), std::make_unique<NodeInt>(1, variable->tok));
			}
			if(is_decl) {
				auto last_retain = static_cast<NodeSingleRetain *>(retain->get_last_statement().get());
				last_retain->set_num(std::make_unique<NodeBinaryExpr>(
					token::SUB,
					array_ref->get_size(),
					std::make_unique<NodeInt>(r_initializer_list->size() - 1, variable->tok),
					variable->tok
				));
			}
			return retain;
		}

		// more_lists[4, *] := (ls, ls1) // <-
		if(auto nd_array = variable->cast<NodeNDArrayRef>()) {
			for (int i = 0; i<r_initializer_list->size(); i++) {
				auto new_nd_array_ref = clone_as<NodeNDArrayRef>(nd_array);
				new_nd_array_ref->replace_next_wildcard_with_index(std::make_unique<NodeInt>(i, Token()));
				new_nd_array_ref->ty = new_nd_array_ref->ty->get_element_type();
				retain->add_as_single_retain(std::move(new_nd_array_ref), std::make_unique<NodeInt>(1, variable->tok));
			}
			if(is_decl) {
				auto last_retain = static_cast<NodeSingleRetain *>(retain->get_last_statement().get());
				last_retain->set_num(std::make_unique<NodeBinaryExpr>(
					token::SUB,
					nd_array->get_size(),
					std::make_unique<NodeInt>(r_initializer_list->size() - 1, variable->tok),
					variable->tok
				));
			}
		}

		auto error = CompileError(ErrorType::InternalError, "", "", var->parent->tok);
		error.m_message = "Invalid retain statement. Could not determine retain statement.";
		error.exit();

		return nullptr;

	}

	/// returns ptr to refernce in retrun statement of functioncall if
	/// r_value is a function call and the return type is an object
	/// else will return og r_value
	static NodeAST* get_return_var_ptr(NodeAST* r_value) {
		if(auto func_call = r_value->cast<NodeFunctionCall>()) {
			if(func_call->kind == NodeFunctionCall::Kind::Constructor) {
				return nullptr;
			}
			if(func_call->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
				if(!func_call->get_definition()) {
					auto error = CompileError(ErrorType::TypeError, "", "", func_call->tok);
					error.m_message = "Function call assigned to <Pointer> does not have a definition."
									  " It needs to be defined for memory allocation to work correctly.";
					error.exit();
				}
				return func_call->get_definition()->return_stmts[0]->return_variables[0].get();
			}
		}
		return r_value;
	}

};