//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"


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
 */
class ASTPointerScope : public ASTVisitor {
private:
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
	explicit ASTPointerScope(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {};

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
//		for(const auto & func_def : node.function_definitions) {
//			if(!func_def->visited) func_def->accept(*this);
//		}

		node.reset_function_visited_flag();
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
//				ref->match_data_structure(ptr);
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

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.get_definition(m_program);
		if(node.definition and node.kind != NodeFunctionCall::Kind::Builtin) {
			if(!node.definition->visited) node.definition->accept(*this);
			node.definition->visited = true;
		}
		return &node;
	};

	NodeAST* visit(NodeSingleAssignment &node) override {
		if(node.l_value->is_member_ref()) return &node;

		if(alters_ref_count(&node)) {
			auto new_assign = std::make_unique<NodeSingleAssignment>(std::move(node.l_value), std::move(node.r_value), node.tok);
			auto new_block = std::make_unique<NodeBlock>(node.tok);
			new_block->add_as_stmt(std::move(new_assign));
			auto assign = static_cast<NodeSingleAssignment*>(new_block->get_last_statement().get());
			if(auto del = add_delete(*assign)) {
				new_block->prepend_body(std::move(del));
			}
			if(auto retain = add_retain(assign->l_value.get(), assign->r_value.get(), false)) {
				new_block->append_body(std::move(retain));
			}
			return node.replace_with(std::move(new_block));
		}

//		node.l_value->accept(*this);
//		node.r_value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration &node) override {
//		if(node.variable->name == "self") return &node;
		if(node.variable->is_member()) return &node;

		node.variable->accept(*this);
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

//		if(node.value) {
//			node.value->accept(*this);
//		}
		return &node;
	}

	// if l_value is of type object and r_value is no constructor
	bool alters_ref_count(NodeAST* node) {
		Type* l_value_type = nullptr;
		NodeAST* r_value;
		if(node->get_node_type() == NodeType::SingleDeclaration) {
			auto decl = static_cast<NodeSingleDeclaration*>(node);
			l_value_type = decl->variable->ty->get_element_type();
			if(!decl->value) {
				return false;
			}
			r_value = decl->value.get();
		} else if(node->get_node_type() == NodeType::SingleAssignment) {
			auto assign = static_cast<NodeSingleAssignment*>(node);
			l_value_type = assign->l_value->ty->get_element_type();
			r_value = assign->r_value.get();
		} else {
			return false;
		}

		if(l_value_type->get_type_kind() == TypeKind::Object) {
			if(r_value->get_node_type() == NodeType::FunctionCall) {
				auto func_call = static_cast<NodeFunctionCall*>(r_value);
				if(func_call->kind == NodeFunctionCall::Kind::Constructor) {
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



	std::unique_ptr<NodeBlock> add_delete(NodeSingleAssignment& assign) {
		auto variable = assign.l_value.get();
		auto value = get_return_var_ptr(assign.r_value.get());
		if(!value) return nullptr;

		auto del = std::make_unique<NodeBlock>(assign.tok);

		// ptr := ptr1
		auto l_pointer_ref = cast_node<NodePointerRef>(variable);
		if(l_pointer_ref) {
			del->add_as_single_delete(clone_as<NodeReference>(l_pointer_ref));
			return del;
		}

		// arr[*] := arr1[*]
		auto l_array_ref = cast_node<NodeArrayRef>(variable);
		auto r_array_ref = cast_node<NodeArrayRef>(value);
		if(l_array_ref and r_array_ref) {
			del->add_as_single_delete(clone_as<NodeReference>(l_array_ref));
			return del;
		}

		auto r_initializer_list = cast_node<NodeInitializerList>(value);


		return del;
	}

	/// assuming variable is of type object
	std::unique_ptr<NodeBlock> add_retain(NodeReference* var, NodeAST* val, bool is_decl = true) {
		auto variable = var;
		auto value = get_return_var_ptr(val);

		if(!value) return nullptr;

		auto l_pointer_ref = cast_node<NodePointerRef>(variable);
		if(l_pointer_ref) {
			return retain_ptr_ptr(l_pointer_ref);
		}

		auto l_array_ref = cast_node<NodeArrayRef>(variable);
		auto r_initializer_list = cast_node<NodeInitializerList>(value);
		if(l_array_ref and r_initializer_list) {
			return retain_arr_init(l_array_ref, r_initializer_list, is_decl);
		}

		auto r_array_ref = cast_node<NodeArrayRef>(value);
		if(l_array_ref and r_array_ref) {
			return retain_arr_arr(l_array_ref);
		}

		return nullptr;



	}

private:
	/// returns ptr to refernce in retrun statement of functioncall if
	/// r_value is a function call and the return type is an object
	/// else will return og r_value
	NodeAST* get_return_var_ptr(NodeAST* r_value) {
		if(r_value->get_node_type() == NodeType::FunctionCall) {
			auto func_call = static_cast<NodeFunctionCall*>(r_value);
			if(func_call->kind == NodeFunctionCall::Kind::Constructor) {
				return nullptr;
			}
			if(func_call->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
				if(!func_call->definition) {
					auto error = CompileError(ErrorType::TypeError, "", "", func_call->tok);
					error.m_message = "Function call assigned to <Pointer> does not have a definition."
									  " It needs to be defined for memory allocation to work correctly.";
					error.exit();
				}
				return func_call->definition->return_stmts[0]->return_variables[0].get();
			}
		}
		return r_value;
	}
	std::unique_ptr<NodeBlock> retain_ptr_ptr(NodePointerRef* l_ref) {
		auto retain = std::make_unique<NodeBlock>(l_ref->tok);
		retain->add_as_single_retain(clone_as<NodeReference>(l_ref), std::make_unique<NodeInt>(1, l_ref->tok));
		return retain;
	}
	std::unique_ptr<NodeBlock> retain_arr_init(NodeArrayRef* l_ref, NodeInitializerList* r_ref, bool is_decl = true) {
		auto arr_ref = clone_as<NodeArrayRef>(l_ref);
		arr_ref->ty = l_ref->ty->get_element_type();
		arr_ref->set_index(std::make_unique<NodeInt>(0, l_ref->tok));
		// 1. initializer list with one element declare lists[10]: List[] := (ls)
		// retain whole array
		if(r_ref->size() == 1) {
			auto retain = std::make_unique<NodeBlock>(l_ref->tok);
			retain->add_as_single_retain(std::move(arr_ref), l_ref->get_size());
			return retain;
		} else {
			// 2. initializer list with multiple elements
			// retain each element
			auto retain = std::make_unique<NodeBlock>(l_ref->tok);
			for(int i=0; i<r_ref->size(); i++) {
				arr_ref->set_index(std::make_unique<NodeInt>(i, l_ref->tok));
				retain->add_as_single_retain(clone_as<NodeReference>(arr_ref.get()), std::make_unique<NodeInt>(1, l_ref->tok));
			}
			if(is_decl) {
				auto last_retain = static_cast<NodeSingleRetain *>(retain->get_last_statement().get());
				last_retain->set_num(std::make_unique<NodeBinaryExpr>(
					token::SUB,
					l_ref->get_size(),
					std::make_unique<NodeInt>(r_ref->size() - 1, l_ref->tok),
					l_ref->tok
				));
			}
			return retain;
		}
	}
	std::unique_ptr<NodeBlock> retain_arr_arr(NodeArrayRef* l_ref) {
		// 3. variable is array -> copy into retain stmt
		auto retain = std::make_unique<NodeBlock>(l_ref->tok);
		retain->add_as_single_retain(clone_as<NodeReference>(l_ref), std::make_unique<NodeInt>(1, l_ref->tok));
		return retain;
	}

};