//
// Created by Mathias Vatter on 27.10.24.
//

#pragma once

#include "ASTDataStructures.h"
#include "../ASTVisitor/ASTVisitor.h"

class NodeStructCreateRefCountFunctions {
private:
	NodeStruct& m_struct;
	Token tok;
	std::unique_ptr<NodeFunctionDefinition> m_del_func;
	std::unique_ptr<NodeFunctionDefinition> m_decr_func;
	std::unique_ptr<NodeFunctionDefinition> m_incr_func;
	std::unique_ptr<NodeVariable> m_num_refs; // num_refs : int
	std::unique_ptr<NodeReference> m_num_refs_ref;
	std::unique_ptr<NodeReference> m_self_ref; // self
	std::unique_ptr<NodeReference> m_alloc_ref; // List::allocation[self]
	std::unique_ptr<NodeReference> m_stack_top_ref; // List::stack_top
	std::unique_ptr<NodeArrayRef> m_stack_ref; // List::stack[]
	int m_num_recursive_members = 0;
	std::vector<NodeDataStructure*> m_recursive_member_structs;
	std::vector<NodeDataStructure*> m_non_recursive_member_structs;
public:
	explicit NodeStructCreateRefCountFunctions(NodeStruct& strct) : m_struct(strct) {
		tok = m_struct.tok;

		// num_refs
		m_num_refs = std::make_unique<NodeVariable>(
			std::nullopt,
			"num_refs",
			TypeRegistry::Integer,
			DataType::Mutable,
			tok
		);

		m_num_refs_ref = m_num_refs->to_reference();
		m_num_refs_ref->match_data_structure(m_num_refs.get());

		m_del_func = get_base_func(m_struct.name + OBJ_DELIMITER + "__del__");
		m_decr_func = get_base_func(m_struct.name + OBJ_DELIMITER + "__decr__", m_num_refs->clone());
		m_incr_func = get_base_func(m_struct.name + OBJ_DELIMITER + "__incr__", m_num_refs->clone());

		// self
		m_self_ref = m_struct.node_self->to_reference();
		m_self_ref->match_data_structure(m_struct.node_self.get());
		m_self_ref->ty = m_struct.node_self->ty;

		// List::allocation[self]
		m_alloc_ref = m_struct.allocation_var->to_reference();
		static_cast<NodeArrayRef*>(m_alloc_ref.get())->set_index(m_self_ref->clone());
		m_alloc_ref->ty = TypeRegistry::Integer;

		// List::stack_top
		m_stack_top_ref = m_struct.stack_top_var->to_reference();
		m_stack_top_ref->match_data_structure(m_struct.stack_top_var);

		// List::stack[]
		m_stack_ref = std::unique_ptr<NodeArrayRef>(static_cast<NodeArrayRef*>(m_struct.stack_var->to_reference().release()));
		m_stack_ref->ty = TypeRegistry::Integer;
		m_stack_ref->set_index(m_stack_top_ref->clone());

		m_num_recursive_members = m_struct.recursive_structs.size();
		// recursive member objects need to be at the end of the member objects vector
		collect_recursive_and_non_recursive_member_structs();

	}


	std::unique_ptr<NodeFunctionDefinition> create_destructor() {
		// if(List::allocation[self] <= 0)
		auto node_if = std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(
				token::LESS_EQUAL,
				m_alloc_ref->clone(),
				std::make_unique<NodeInt>(0, tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		// set everything to nil
		for(auto &mem : m_non_recursive_member_structs) {
			node_if->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
				to_member_chain_ref(mem),
				std::make_unique<NodeNil>(tok),
				tok
			));
		}
		for(auto &mem : m_recursive_member_structs) {
			node_if->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
				to_member_chain_ref(mem),
				std::make_unique<NodeNil>(tok),
				tok
			));
		}
		m_del_func->body->add_as_stmt(std::move(node_if));
		m_del_func->parent = &m_struct;
		m_del_func->ty = TypeRegistry::Void;
		return std::move(m_del_func);
	}

	std::unique_ptr<NodeFunctionDefinition> create_decr_function() {
		// is linear recursive
//		if(m_recursive_member_structs.size() <= 1) {
//			return create_decr_without_stack();
//		} else {
			return create_decr_with_stack();
//		}
	}

	std::unique_ptr<NodeFunctionDefinition> create_decr_with_stack() {
		m_self_ref->declaration = get_self_ptr(m_decr_func.get());
		m_num_refs_ref->declaration = get_num_refs_ptr(m_decr_func.get());
		auto &func_body = m_decr_func->body;
		// Node::stack[Node::stack_top] := self
		func_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(m_stack_ref.get()),
			m_self_ref->clone(),
			tok
		));
		// inc(Node::stack_top)
		func_body->add_as_stmt(DefinitionProvider::inc(clone_as<NodeReference>(m_stack_top_ref.get())));

		// while Node::stack_top > 0
		auto node_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(
				token::GREATER_THAN,
				m_stack_top_ref->clone(),
				std::make_unique<NodeInt>(0, tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		// dec(Node::stack_top)
		node_while->body->add_as_stmt(DefinitionProvider::dec(clone_as<NodeReference>(m_stack_top_ref.get())));
		// self := Node::stack[Node::stack_top]
		node_while->body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(m_self_ref.get()),
			m_stack_ref->clone(),
			tok
		));
		// if (self = nil) continue
		auto self_check = std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(
				token::EQUAL,
				m_self_ref->clone(),
				std::make_unique<NodeNil>(tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		self_check->if_body->add_as_stmt(DefinitionProvider::continu(tok));
		node_while->body->add_as_stmt(std::move(self_check));
		// Node::allocation[self] := Node::allocation[self] + num_refs
		node_while->body->add_as_stmt(dec_alloc_array());
		// if(Node::allocation[self] > 0) continue
		node_while->body->add_as_stmt(alloc_array_check_continue());

		// if there are object members -> continue recursively
		if(!m_recursive_member_structs.empty() or !m_non_recursive_member_structs.empty()) {
			for(auto &mem : m_non_recursive_member_structs) {
				// if(current.other_struct # nil)
				auto nil_check = ASTVisitor::make_nil_check(to_member_chain_ref(mem));
				nil_check->if_body->add_as_stmt(std::make_unique<NodeFunctionCall>(
					false,
					std::make_unique<NodeFunctionHeader>(
						mem->ty->get_element_type()->to_string() + OBJ_DELIMITER + "__del__",
						std::make_unique<NodeParamList>(mem->tok, mem->clone()),
						mem->tok
					),
					mem->tok
				));
				nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
					to_member_chain_ref(mem),
					std::make_unique<NodeNil>(mem->tok),
					mem->tok
				));
				node_while->body->add_as_stmt(std::move(nil_check));
			}
			for(auto & mem : m_recursive_member_structs) {
				// if(self.next # nil)
				auto nil_check = ASTVisitor::make_nil_check(to_member_chain_ref(mem));
				// inc(Node::stack_top)
				nil_check->if_body->add_as_stmt(DefinitionProvider::inc(clone_as<NodeReference>(m_stack_top_ref.get())));
				// Node::stack[Node::stack_top] := self.next
				nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
					clone_as<NodeReference>(m_stack_ref.get()),
					to_member_chain_ref(mem)->clone(),
					mem->tok
				));
				node_while->body->add_as_stmt(std::move(nil_check));
			}

		}
		// add actual delete function
		node_while->body->add_as_stmt(std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				m_struct.name + OBJ_DELIMITER + "__del__",
				std::make_unique<NodeParamList>(tok, m_self_ref->clone()),
				tok
			),
			tok
		));

		m_decr_func->body->add_as_stmt(std::move(node_while));
		m_decr_func->parent = &m_struct;
		m_decr_func->ty = TypeRegistry::Void;
		return std::move(m_decr_func);

	}


	std::unique_ptr<NodeFunctionDefinition> create_decr_without_stack() {
		m_self_ref->declaration = get_self_ptr(m_decr_func.get());
		m_num_refs_ref->declaration = get_num_refs_ptr(m_decr_func.get());
		// declare current
		auto current_decl = std::make_unique<NodeSingleDeclaration>(
			std::make_unique<NodeVariable>(
				std::nullopt,
				"current",
				m_struct.node_self->ty,
				DataType::Mutable,
				tok
			),
			m_self_ref->clone(), tok
		);
		current_decl->variable->is_local = true;
		auto current_ref = current_decl->variable->to_reference();
		current_ref->match_data_structure(current_decl->variable.get());
		m_decr_func->body->add_as_stmt(std::move(current_decl));

		// while self # nil
		auto outer_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(
				token::NOT_EQUAL,
				current_ref->clone(),
				std::make_unique<NodeNil>(tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);

		outer_while->body->add_as_stmt(dec_alloc_array(current_ref->clone()));
		auto check_for_deletion = alloc_array_check_continue();
		// current := nil
		check_for_deletion->if_body->prepend_stmt(
			std::make_unique<NodeStatement>(
				std::make_unique<NodeSingleAssignment>(
					clone_as<NodeReference>(current_ref.get()),
					std::make_unique<NodeNil>(tok),
					tok
				),
				tok
			)
		);
		outer_while->body->add_as_stmt(std::move(check_for_deletion));

		// if there are object members -> continue recursively
		if(!m_recursive_member_structs.empty() or !m_non_recursive_member_structs.empty()) {
			for(auto &mem : m_non_recursive_member_structs) {
				// if(current.other_struct # nil)
				auto nil_check = ASTVisitor::make_nil_check(to_member_chain_ref(mem, current_ref.get()));
				nil_check->if_body->add_as_stmt(std::make_unique<NodeFunctionCall>(
					false,
					std::make_unique<NodeFunctionHeader>(
						mem->ty->get_element_type()->to_string() + OBJ_DELIMITER + "__del__",
						std::make_unique<NodeParamList>(mem->tok, mem->clone()),
						mem->tok
					),
					mem->tok
				));
				nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
					to_member_chain_ref(mem),
					std::make_unique<NodeNil>(mem->tok),
					mem->tok
				));
				outer_while->body->add_as_stmt(std::move(nil_check));
			}
			for(auto & mem : m_recursive_member_structs) {
				// self := current <- save the current object for deletion
				outer_while->body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
					clone_as<NodeReference>(m_self_ref.get()),
					current_ref->clone(),
					tok
				));
				// current.next
				auto mem_ref = to_member_chain_ref(mem, current_ref.get());
				// if(current.next # nil)
				auto nil_check = ASTVisitor::make_nil_check(clone_as<NodeReference>(mem_ref.get()));
				// current := current.next
				nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
					clone_as<NodeReference>(current_ref.get()),
					mem_ref->clone(),
					mem->tok
				));
				// else : current := nil
				nil_check->else_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
					clone_as<NodeReference>(current_ref.get()),
					std::make_unique<NodeNil>(mem->tok),
					mem->tok
				));
				outer_while->body->add_as_stmt(std::move(nil_check));

			}

		}
		// add actual delete function
		outer_while->body->add_as_stmt(std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				m_struct.name + OBJ_DELIMITER + "__del__",
				std::make_unique<NodeParamList>(tok, m_self_ref->clone()),
				tok
			),
			tok
		));

		m_decr_func->body->add_as_stmt(std::move(outer_while));
		m_decr_func->parent = &m_struct;
		m_decr_func->ty = TypeRegistry::Void;
		return std::move(m_decr_func);
	}


private:

	std::unique_ptr<NodeFunctionDefinition> get_base_func(const std::string& name, std::unique_ptr<NodeAST> add_param = nullptr) {
		auto func_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				name,
				std::make_unique<NodeParamList>(
					tok,
					m_struct.node_self->clone()
				),
				tok
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		if(add_param) {
			func_def->header->args->add_param(std::move(add_param));
		}
		return func_def;
	}

	std::unique_ptr<NodeReference> to_member_chain_ref(NodeDataStructure* mem, NodeReference* idx = nullptr) {
		auto ref = mem->to_reference();
		ref->remove_obj_prefix();
		ref->match_data_structure(mem);
		ref->ty = mem->ty;
		auto mem_access_chain = std::make_unique<NodeAccessChain>(mem->tok, idx ? idx->clone() : m_self_ref->clone(), std::move(ref));
		mem_access_chain->ty = mem->ty;
		return mem_access_chain;
	}

	void collect_recursive_and_non_recursive_member_structs() {
		std::unordered_set<std::string> recursive_structs;
		for(auto &strct : m_struct.recursive_structs) {
			recursive_structs.insert(strct->name);
		}
		for(auto &mem : m_struct.member_table) {
			if(mem.first == "self") continue;
			if(mem.second->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
				auto mem_type = mem.second->ty->get_element_type();
				if(recursive_structs.find(mem_type->to_string()) != recursive_structs.end()) {
					m_recursive_member_structs.push_back(mem.second);
				} else {
					m_non_recursive_member_structs.push_back(mem.second);
				}
			}
		}
	}

	static NodeDataStructure* get_self_ptr(NodeFunctionDefinition* func_def) {
		return static_cast<NodeDataStructure*>(func_def->header->args->params[0].get());
	}
	static NodeDataStructure* get_num_refs_ptr(NodeFunctionDefinition* func_def) {
		return static_cast<NodeDataStructure*>(func_def->header->args->params.back().get());
	}

	///	List::allocation[self] := List::allocation[self] - num_refs
	std::unique_ptr<NodeSingleAssignment> dec_alloc_array(std::unique_ptr<NodeAST> idx=nullptr) {
		auto alloc_ref = clone_as<NodeArrayRef>(m_alloc_ref.get());
		if(idx) {
			alloc_ref->set_index(std::move(idx));
		}
		return std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(alloc_ref.get()),
			std::make_unique<NodeBinaryExpr>(
				token::SUB,
				std::move(alloc_ref),
				m_num_refs_ref->clone(),
				tok
			),
			tok
		);
	}

	///	if (List::allocation[self] > 0)
	///		continue
	///	end if
	std::unique_ptr<NodeIf> alloc_array_check_continue() {
		auto node_if = std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(
				token::GREATER_THAN,
				std::move(m_alloc_ref),
				std::make_unique<NodeInt>(0, tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		node_if->if_body->add_as_stmt(DefinitionProvider::continu(tok));
		return node_if;
	}

};