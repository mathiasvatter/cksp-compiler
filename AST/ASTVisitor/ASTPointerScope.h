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
		if(ref.declaration->ty != ref.ty) {
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
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}

		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeBlock &node) override {
		node.flatten();

		if(node.scope) m_pointer_scope_stack.emplace_back();
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		// add delete statements for all local pointers
		if(node.scope) {
			auto local_ptrs = remove_scope();
			for(auto & [key, ptr] : local_ptrs) {
				auto ref = ptr->to_reference();
				ref->match_data_structure(ptr);
				auto del = std::make_unique<NodeSingleDelete>(std::move(ref), ptr->tok);
				node.add_stmt(std::make_unique<NodeStatement>(std::move(del), ptr->tok));
			}
		}
		return &node;
	}

	NodeAST* visit(NodeSingleDelete &node) override {
		node.ptr->accept(*this);
		remove_pointer(*static_cast<NodeReference*>(node.ptr.get()));
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment &node) override {
		node.has_object = alters_ref_count(&node);

//		node.l_value->accept(*this);
//		node.r_value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration &node) override {
		node.has_object = alters_ref_count(&node);

		node.variable->accept(*this);
		if(node.variable->is_local and node.variable->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			m_pointer_scope_stack.back()[{node.variable->name, node.variable->ty}] = node.variable.get();
		}

		if(alters_ref_count(&node)) {
			if(auto retain = add_retain(node)) {
				node.set_retain(std::move(retain));
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
			return true;
		}
		return false;
	}

	/// assuming variable is of type object
	std::unique_ptr<NodeRetain> add_retain(NodeSingleDeclaration& declaration) {
		if(!declaration.value) return nullptr;
		auto &variable = declaration.variable;
		auto &value = declaration.value;

		// 1. if variable is not composite type -> easiest case
		if(variable->ty->get_type_kind() != TypeKind::Composite) {
			auto retain = std::make_unique<NodeRetain>(variable->tok);
			retain->add_single_retain(variable->to_reference(), std::make_unique<NodeInt>(1, value->tok));
//			declaration.set_retain(std::move(retain));
			return retain;
		}

		// 2. value is initializer list
		// two cases here: initializer list with one element or multiple elements
		if(value->get_node_type() == NodeType::InitializerList) {
			if(variable->get_node_type() != NodeType::Array) {
				auto error = CompileError(ErrorType::InternalError, "Expected array as variable in initializer list", "", variable->tok);
				error.exit();
			}
			auto ref = std::unique_ptr<NodeArrayRef>(static_cast<NodeArrayRef*>(variable->to_reference().release()));
			auto ref_size = DefinitionProvider::num_elements(clone_as<NodeReference>(ref.get()));
			ref->set_index(std::make_unique<NodeInt>(0, value->tok));
			ref->ty = variable->ty->get_element_type();
			auto init_list = static_cast<NodeInitializerList*>(value.get());
			// 1. initializer list with one element declare lists[10]: List[] := (ls)
			// retain whole array
			if(init_list->size() == 1) {
				auto retain = std::make_unique<NodeRetain>(variable->tok);
				retain->add_single_retain(clone_as<NodeReference>(ref.get()), std::move(ref_size));
//				declaration.set_retain(std::move(retain));
				return retain;
			} else {
				// 2. initializer list with multiple elements
				// retain each element
				auto retain = std::make_unique<NodeRetain>(variable->tok);
				for(int i=0; i<init_list->elements.size(); i++) {
					ref->set_index(std::make_unique<NodeInt>(i, value->tok));
					retain->add_single_retain(clone_as<NodeReference>(ref.get()), std::make_unique<NodeInt>(1, value->tok));
				}
				retain->ptrs.back()->set_num(std::make_unique<NodeBinaryExpr>(
					token::SUB,
					std::move(ref_size),
					std::make_unique<NodeInt>(init_list->size()-1, value->tok),
					value->tok
					));
//				declaration.set_retain(std::move(retain));
				return retain;
			}

		}



		if(variable->ty->get_type_kind() != TypeKind::Composite) {
			// three cases: initializer list with one element:
			// declare lists[10]: List[] := (ls)

			auto retain = std::make_unique<NodeRetain>(variable->tok);
			retain->add_single_retain(variable->to_reference(), std::make_unique<NodeInt>(1, value->tok));
//			declaration.set_retain(std::move(retain));
			return retain;
		}
		return nullptr;

	}

//	/// replaces node with a block with retain statement and declaration/assign statement if applicable
//	NodeAST* add_retain_stmt(NodeAST& node) {
//		// node can either be a declaration or an assignment
//		auto retain = std::make_unique<NodeRetain>(node.tok);
//		// 1. when it is declaration, check that variable is Object
//		if(node.parent->get_node_type() == NodeType::Declaration) {
//			auto decl = static_cast<NodeSingleDeclaration*>(node.parent);
//			auto &variable = decl->variable;
//			auto &value = decl->value;
//			// 1. if variable is not object type, return
//			if(variable->ty->get_element_type()->get_type_kind() != TypeKind::Object) {
//				return &node;
//			}
//			// 2. if there is no value assigned, return
//			if(!value) {
//				return &node;
//			}
//			// 3. if value is constructor, return as well, since retain is already called in constructor
//			// 4. if value is function call and returns object type -> retain
//			if(value->get_node_type() == NodeType::FunctionCall) {
//				auto func_call = static_cast<NodeFunctionCall*>(value.get());
//				if(func_call->kind == NodeFunctionCall::Kind::Constructor) {
//					return &node;
//				} else if(func_call->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
//					retain->add_single_retain(variable->to_reference(), std::make_unique<NodeInt>(1, value->tok));
//					decl->set_retain(std::move(retain));
//					return &node;
//				}
//			}
//			// 5. if variable is pointer, then call retain one time
//			if(variable->get_node_type() == NodeType::Pointer) {
//				if(auto ref = clone_as<NodeReference>(value.get())) {
//					retain->add_single_retain(std::move(ref), std::make_unique<NodeInt>(1, value->tok));
//					decl->set_retain(std::move(retain));
//					return &node;
//				} else {
//					auto error = CompileError(ErrorType::InternalError, "Expected reference as value in pointer declaration", "", node.tok);
//					error.exit();
//				}
//			}
//			// 6. if value is initializer list, variable is array
//			// if initializer list is only one value, retain num_elements(array) times
//		}
//
//		// 2. check if is r_value in an assignment statement ->
//		if(node.parent->get_node_type() == NodeType::Assignment) {
//			auto assign = static_cast<NodeSingleAssignment*>(node.parent);
//		}
//
//	}

};