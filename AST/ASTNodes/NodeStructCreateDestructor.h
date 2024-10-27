//
// Created by Mathias Vatter on 27.10.24.
//

#pragma once

#include "ASTDataStructures.h"
#include "../ASTVisitor/ASTVisitor.h"

class NodeStructCreateDestructor {
private:
	NodeStruct& m_struct;
	Token tok;
	std::string m_func_name;
	std::unique_ptr<NodeFunctionDefinition> m_base_func;
	std::unique_ptr<NodeReference> m_self_ref;
	std::unique_ptr<NodeReference> m_alloc_ref;
	std::unique_ptr<NodeArrayRef> m_stack_ref;
	std::vector<NodeDataStructure*> m_member_objects; // collect all members that are objects
	int m_num_recursive_members = 0;
	std::vector<std::unique_ptr<NodeReference>> m_member_refs;
public:
	explicit NodeStructCreateDestructor(NodeStruct& strct) : m_struct(strct) {
		tok = m_struct.tok;
		m_func_name = m_struct.name+OBJ_DELIMITER+"__del__";
		m_base_func = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				m_func_name,
				std::make_unique<NodeParamList>(tok, m_struct.node_self->clone()),
				tok
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(tok, true),
			tok
		);

		// self
		m_self_ref = static_cast<NodeDataStructure*>(m_base_func->header->args->params[0].get())->to_reference();
		m_self_ref->match_data_structure(m_struct.node_self.get());
		m_self_ref->ty = m_struct.node_self->ty;

		// List::allocation[self]
		m_alloc_ref = m_struct.allocation_var->to_reference();
		static_cast<NodeArrayRef*>(m_alloc_ref.get())->set_index(m_self_ref->clone());
		m_alloc_ref->ty = TypeRegistry::Integer;

		// List::stack[]
		m_stack_ref = std::unique_ptr<NodeArrayRef>(static_cast<NodeArrayRef*>(m_struct.stack_var->to_reference().release()));
		m_stack_ref->ty = TypeRegistry::Integer;

		// recursive member objects need to be at the end of the member objects vector
		std::vector<NodeDataStructure*> recursive_member_objects;
		// collect all member objects
		for(auto &mem : m_struct.member_table) {
			if(mem.first == "self") continue;
			if(mem.second->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
				if(mem.second->ty->get_element_type()->to_string() == m_struct.name) {
					recursive_member_objects.push_back(mem.second);
				} else {
					m_member_objects.push_back(mem.second);
				}
			}
		}
		m_num_recursive_members = recursive_member_objects.size();
		// add recursive member objects at the end
		m_member_objects.insert(m_member_objects.end(), recursive_member_objects.begin(), recursive_member_objects.end());

		// create reference for recursive member objects
		for(auto &mem : m_member_objects) {
			auto ref = mem->to_reference();
			ref->remove_obj_prefix();
			ref->match_data_structure(mem);
			ref->ty = mem->ty;
			auto mem_access_chain = std::make_unique<NodeAccessChain>(mem->tok, m_self_ref->clone(), std::move(ref));
			mem_access_chain->ty = mem->ty;
			m_member_refs.push_back(std::move(mem_access_chain));
		}
	}
	std::unique_ptr<NodeFunctionDefinition> create_destructor() {
		if(m_num_recursive_members < 2) {
			return create_destructor_without_stack();
		} else {
			return create_destructor_with_stack();
		}
	}

	std::unique_ptr<NodeFunctionDefinition> create_destructor_with_stack() {
		auto stack_top_decl = std::make_unique<NodeSingleDeclaration>(
			std::make_unique<NodeVariable>(
				std::nullopt,
				"stack_top",
				TypeRegistry::Integer,
				DataType::Mutable,
				tok
			),
			nullptr, tok
		);
		stack_top_decl->variable->is_local = true;
		auto stack_top_ref = stack_top_decl->variable->to_reference();
		stack_top_ref->ty = TypeRegistry::Integer;
		stack_top_ref->match_data_structure(stack_top_decl->variable.get());

	}

	std::unique_ptr<NodeFunctionDefinition> create_destructor_without_stack() {
		// while self # nil
		auto outer_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(
				token::NOT_EQUAL,
				m_self_ref->clone(),
				std::make_unique<NodeNil>(tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		// self := nil last statement in outer while block
		auto last_assignment = std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(m_self_ref.get()),
			std::make_unique<NodeNil>(tok),
			tok
		);

		outer_while->body->add_as_stmt(dec_alloc_array());

		// if there are object members -> continue recursively
		if(!m_member_refs.empty()) {
			auto inner_if = alloc_array_check();
			for(auto &mem : m_member_refs) {
				std::string prefix = mem->ty->get_element_type()->to_string();
				// if(self.other_struct # nil)
				auto nil_check = ASTVisitor::make_nil_check(clone_as<NodeReference>(mem.get()));
				// check for recursive data structure
				if(prefix == m_struct.name)  {
					// self := self.left
					nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
						clone_as<NodeReference>(m_self_ref.get()),
						mem->clone(),
						mem->tok
					));
					// self.left := nil
					nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
						clone_as<NodeReference>(mem.get()),
						std::make_unique<NodeNil>(mem->tok),
						mem->tok
					));
					nil_check->if_body->add_as_stmt(DefinitionProvider::continu(mem->tok));
				} else {
					nil_check->if_body->add_as_stmt(std::make_unique<NodeFunctionCall>(
						false,
						std::make_unique<NodeFunctionHeader>(
							prefix+OBJ_DELIMITER+"__del__",
							std::make_unique<NodeParamList>(mem->tok, mem->clone()),
							mem->tok
						),
						mem->tok
					));
					nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
						std::move(mem),
						std::make_unique<NodeNil>(mem->tok),
						mem->tok
					));
				}
				inner_if->if_body->add_as_stmt(std::move(nil_check));
			}
			outer_while->body->add_as_stmt(std::move(inner_if));
		}
		outer_while->body->add_as_stmt(std::move(last_assignment));
		m_base_func->body->add_as_stmt(std::move(outer_while));
		m_base_func->parent = &m_struct;
		m_base_func->ty = TypeRegistry::Void;
		return std::move(m_base_func);
	}


private:

	///	dec(List::allocation[self])
	std::unique_ptr<NodeFunctionCall> dec_alloc_array() {
		return DefinitionProvider::dec(clone_as<NodeReference>(m_alloc_ref.get()));
	}

	///	if (List::allocation[self] <= 0)
	///		...
	///	end if
	std::unique_ptr<NodeIf> alloc_array_check() {
		return std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(
				token::LESS_EQUAL,
				std::move(m_alloc_ref),
				std::make_unique<NodeInt>(0, tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			std::make_unique<NodeBlock>(tok, true),
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