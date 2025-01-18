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
	std::shared_ptr<NodeVariable> m_num_refs; // num_refs : int
	std::unique_ptr<NodeReference> m_num_refs_ref;
	std::unique_ptr<NodeReference> m_self_ref; // self
	std::unique_ptr<NodeReference> m_alloc_ref; // List::allocation[self]
	std::unique_ptr<NodeReference> m_stack_top_ref; // List::stack_top
	std::unique_ptr<NodeArrayRef> m_stack_ref; // List::stack[]
	std::unique_ptr<NodeReference> m_iterator_ref;
	std::vector<std::shared_ptr<NodeDataStructure>> m_recursive_member_structs;
	std::vector<std::shared_ptr<NodeDataStructure>> m_non_recursive_member_structs;
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
		m_num_refs_ref->match_data_structure(m_num_refs);

		m_del_func = get_base_func(m_struct.name + OBJ_DELIMITER + "__del__");
		m_decr_func = get_base_func(m_struct.name + OBJ_DELIMITER + "__decr__", clone_as<NodeDataStructure>(m_num_refs.get()));
		m_incr_func = get_base_func(m_struct.name + OBJ_DELIMITER + "__incr__", clone_as<NodeDataStructure>(m_num_refs.get()));

		// self
		m_self_ref = m_struct.node_self->to_reference();
		m_self_ref->match_data_structure(m_struct.node_self);
		m_self_ref->ty = m_struct.node_self->ty;

		// List::allocation[self]
		m_alloc_ref = m_struct.allocation_var->to_reference();
		m_alloc_ref->cast<NodeArrayRef>()->set_index(m_self_ref->clone());
		m_alloc_ref->ty = TypeRegistry::Integer;

		// List::stack_top
		m_stack_top_ref = m_struct.stack_top_var->to_reference();
		m_stack_top_ref->match_data_structure(m_struct.stack_top_var);

		// List::stack[]
		m_stack_ref = unique_ptr_cast<NodeArrayRef>(m_struct.stack_var->to_reference());
		m_stack_ref->ty = TypeRegistry::Integer;
		m_stack_ref->set_index(m_stack_top_ref->clone());

		m_iterator_ref = ASTVisitor::get_iterator_var(tok)->to_reference();

		// recursive member objects need to be at the end of the member objects vector
		collect_recursive_and_non_recursive_member_structs();

	}

	/// returns iterator declaration for loops and sets m_iterator_ref to declaration
	std::unique_ptr<NodeSingleDeclaration> get_iterator_declaration() {
		auto decl = std::make_unique<NodeSingleDeclaration>(
			ASTVisitor::get_iterator_var(tok),
			nullptr,
			tok
		);
		m_iterator_ref->declaration = decl->variable;
		return decl;
	}

	std::unique_ptr<NodeFunctionDefinition> create_incr_function() {
		set_self_ref_declaration(m_incr_func.get());
		set_num_refs_ref_declaration(m_incr_func.get());
		auto &func_body = m_incr_func->body;
		auto nil_check = ASTVisitor::make_nil_check(clone_as<NodeReference>(m_self_ref.get()));
		// List::allocation[self] := List::allocation[self] + num_refs
		nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(m_alloc_ref.get()),
			std::make_unique<NodeBinaryExpr>(
				token::ADD,
				m_alloc_ref->clone(),
				m_num_refs_ref->clone(),
				tok
			),
			tok
		));
		func_body->add_as_stmt(std::move(nil_check));
		m_incr_func->parent = &m_struct;
		m_incr_func->header->create_function_type(TypeRegistry::Void);
		m_incr_func->ty = TypeRegistry::Void;
		m_incr_func->collect_references();
		return std::move(m_incr_func);
	}

	/// checks if member is of composite type -> if yes wraps block in loop and iterates over array
	void wrap_in_loop(std::unique_ptr<NodeStatement>& stmt, std::shared_ptr<NodeDataStructure> mem, std::shared_ptr<NodeDataStructure> iterator) {
		if(mem->ty->cast<CompositeType>()) {
			auto block = std::make_unique<NodeBlock>(stmt->tok);
			block->add_as_stmt(std::move(stmt->statement));
			block->wrap_in_loop(iterator, std::make_unique<NodeInt>(0, tok), static_cast<NodeComposite*>(mem.get())->get_size(), false);
//			block->get_last_statement()->desugar(nullptr);
			stmt->set_statement(std::move(block));
		}
	}

	std::unique_ptr<NodeFunctionDefinition> create_delete() {
		set_self_ref_declaration(m_del_func.get());
		auto iter_decl = get_iterator_declaration();
		// if(self # nil)
		auto nil_check = ASTVisitor::make_nil_check(clone_as<NodeReference>(m_self_ref.get()));
		// // set everything to nil
		// for(auto &mem : m_non_recursive_member_structs) {
		// 	nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
		// 		to_member_chain_ref(mem),
		// 		std::make_unique<NodeNil>(tok),
		// 		tok
		// 	));
		// 	wrap_in_loop(nil_check->if_body->statements[0], mem, iter_decl->variable);
		// }
		for (auto &mem : m_struct.member_table) {
			auto member = mem.second.lock();
			if (member->is_engine) continue;
			if (member == m_struct.node_self) continue;
			auto assignment = std::make_unique<NodeSingleAssignment>(
				to_member_chain_ref(member),
				TypeRegistry::get_neutral_element_from_type(member->ty),
				tok
			);
			nil_check->if_body->add_as_stmt(std::move(assignment));
			wrap_in_loop(nil_check->if_body->statements.back(), member, iter_decl->variable);
		}
		// for(auto &mem : m_recursive_member_structs) {
		// 	nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
		// 		to_member_chain_ref(mem),
		// 		std::make_unique<NodeNil>(tok),
		// 		tok
		// 	));
		// 	wrap_in_loop(nil_check->if_body->statements[0], mem, iter_decl->variable);
		// }
		m_del_func->body->add_as_stmt(std::move(iter_decl));
		m_del_func->body->add_as_stmt(std::move(nil_check));
		m_del_func->parent = &m_struct;
		m_del_func->ty = TypeRegistry::Void;
		m_del_func->header->create_function_type(TypeRegistry::Void);
		m_del_func->collect_references();
		return std::move(m_del_func);
	}

	/// returns true if struct has only one recursive member and this member is itself
	/// this member must not be of composite type
	[[nodiscard]] bool is_linear_recursive() const {
		bool is_non_recursive = m_struct.recursive_structs.empty();
		if(is_non_recursive) return true;

		bool is_linear_recursive = m_struct.recursive_structs.size() == 1 and m_recursive_member_structs.size() == 1;
		// the linear recursive member must not be of composite type
		if(is_linear_recursive and m_recursive_member_structs[0]->ty->cast<CompositeType>()) {
			is_linear_recursive = false;
		}
		return is_linear_recursive;
	}

	/// returns true if struct has more than one recursive member and these members are itself
	bool is_non_linear_recursive_with_homogenous_types() {
		if(is_linear_recursive()) return false;
		for(auto &mem : m_recursive_member_structs) {
			if(mem->ty->get_element_type() != m_struct.ty) return false;
		}
		return true;
	}

	/// returns true if struct has more than one recursive member and these members are not itself
	/// e.g. List -> Node -> List-> ...
	bool is_non_linear_recursive_with_heterogenous_types() {
		return !is_linear_recursive() and !is_non_linear_recursive_with_homogenous_types();
	}

	std::unique_ptr<NodeFunctionDefinition> create_decr_function() {
		// is linear recursive
		if(is_linear_recursive()) {
			return create_lin_rec_decr();
		} else if(is_non_linear_recursive_with_homogenous_types()) {
			return create_non_lin_rec_homogenous_decr();
		} else {
			return create_non_lin_rec_heterogenous_decr();
		}
	}

	std::unique_ptr<NodeFunctionDefinition> create_non_lin_rec_heterogenous_decr() {
		set_self_ref_declaration(m_decr_func.get());
		set_num_refs_ref_declaration(m_decr_func.get());
		auto &func_body = m_decr_func->body;
		// Node::stack[Node::stack_top] := self
		func_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(m_stack_ref.get()),
			m_self_ref->clone(),
			tok
		));
		// inc(Node::stack_top)
		func_body->add_as_stmt(DefinitionProvider::inc(clone_as<NodeReference>(m_stack_top_ref.get())));
		auto node_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(tok),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		std::unique_ptr<NodeBinaryExpr> condition = nullptr;
		for(auto & rec : m_struct.recursive_structs) {
			// STRUCT::stack_top
			auto stack_var_ref = rec->stack_top_var->to_reference();
			stack_var_ref->match_data_structure(rec->stack_top_var);
			if(!condition) {
				condition = std::make_unique<NodeBinaryExpr>(
					token::GREATER_THAN,
					stack_var_ref->clone(),
					std::make_unique<NodeInt>(0, tok),
					tok
				);
			} else {
				condition = std::make_unique<NodeBinaryExpr>(
					token::BOOL_AND,
					std::move(condition),
					std::make_unique<NodeBinaryExpr>(
						token::GREATER_THAN,
						stack_var_ref->clone(),
						std::make_unique<NodeInt>(0, tok),
						tok
					),
					tok
				);
			}
			auto while_body = rec->generate_ref_count_while(m_self_ref->get_declaration(), m_num_refs_ref->get_declaration());
			node_while->body->add_as_stmt(std::move(while_body));
		}
		node_while->set_condition(std::move(condition));

		m_decr_func->body->add_as_stmt(std::move(node_while));
		m_decr_func->parent = &m_struct;
		m_decr_func->ty = TypeRegistry::Void;
		m_decr_func->header->create_function_type(TypeRegistry::Void);
		m_decr_func->collect_references();
		return std::move(m_decr_func);
	}

	std::unique_ptr<NodeWhile> get_stack_while_loop(std::shared_ptr<NodeDataStructure> self, std::shared_ptr<NodeDataStructure> num_refs) {
		auto iter_decl = get_iterator_declaration();
		m_self_ref->declaration = self;
		m_num_refs_ref->declaration = num_refs;
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
				node_while->body->add_as_stmt(std::make_unique<NodeFunctionCall>(
					false,
					std::make_unique<NodeFunctionHeaderRef>(
						mem->ty->get_element_type()->to_string() + OBJ_DELIMITER + "__decr__",
						std::make_unique<NodeParamList>(mem->tok, to_member_chain_ref(mem), std::make_unique<NodeInt>(1, mem->tok)),
						mem->tok
					),
					mem->tok
				));

				wrap_in_loop(node_while->body->statements.back(), mem, iter_decl->variable);
			}
			for(auto & mem : m_recursive_member_structs) {
				// if(self.next # nil)
				auto nil_check = ASTVisitor::make_nil_check(to_member_chain_ref(mem));
				// inc(Node::stack_top)
				// stack_top has to have the prefix of its type this time!!
				auto typed_stack_top = clone_as<NodeReference>(m_stack_top_ref.get());
				typed_stack_top->remove_obj_prefix();
				typed_stack_top->name = mem->ty->get_element_type()->to_string()+OBJ_DELIMITER+typed_stack_top->name;
				nil_check->if_body->add_as_stmt(DefinitionProvider::inc(clone_as<NodeReference>(typed_stack_top.get())));
				// Node::stack[Node::stack_top] := self.next
				nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
					std::move(typed_stack_top),
					to_member_chain_ref(mem)->clone(),
					mem->tok
				));
				node_while->body->add_as_stmt(std::move(nil_check));
				wrap_in_loop(node_while->body->statements.back(), mem, iter_decl->variable);
			}

		}
		node_while->body->prepend_as_stmt(std::move(iter_decl));
		// add actual delete function
		node_while->body->add_as_stmt(std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				m_struct.name + OBJ_DELIMITER + "__del__",
				std::make_unique<NodeParamList>(tok, m_self_ref->clone()),
				tok
			),
			tok
		));
		return node_while;
	}

	std::unique_ptr<NodeFunctionDefinition> create_non_lin_rec_homogenous_decr() {
		set_self_ref_declaration(m_decr_func.get());
		set_num_refs_ref_declaration(m_decr_func.get());
		auto &func_body = m_decr_func->body;
		// Node::stack[Node::stack_top] := self
		func_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(m_stack_ref.get()),
			m_self_ref->clone(),
			tok
		));
		// inc(Node::stack_top)
		func_body->add_as_stmt(DefinitionProvider::inc(clone_as<NodeReference>(m_stack_top_ref.get())));

		auto node_while = get_stack_while_loop(m_self_ref->get_declaration(), m_num_refs_ref->get_declaration());

		m_decr_func->body->add_as_stmt(std::move(node_while));
		m_decr_func->parent = &m_struct;
		m_decr_func->ty = TypeRegistry::Void;
		m_decr_func->header->create_function_type(TypeRegistry::Void);
		m_decr_func->collect_references();
		return std::move(m_decr_func);
	}

	std::unique_ptr<NodeFunctionDefinition> create_lin_rec_decr() {
		auto iter_decl = get_iterator_declaration();
		set_self_ref_declaration(m_decr_func.get());
		set_num_refs_ref_declaration(m_decr_func.get());
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
//		current_ref->match_data_structure(current_decl->variable);
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
		auto check_for_deletion = alloc_array_check_continue(current_ref->clone());
		// current := nil
		check_for_deletion->if_body->prepend_as_stmt(
			std::make_unique<NodeSingleAssignment>(
				clone_as<NodeReference>(current_ref.get()),
				std::make_unique<NodeNil>(tok),
				tok
			)
		);
		outer_while->body->add_as_stmt(std::move(check_for_deletion));

		// self := current
		outer_while->body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(m_self_ref.get()),
			current_ref->clone(),
			tok
		));
		// current := nil
		outer_while->body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(current_ref.get()),
			std::make_unique<NodeNil>(tok),
			tok
		));
		// if there are object members -> continue recursively
		if(!m_recursive_member_structs.empty() or !m_non_recursive_member_structs.empty()) {
			for(auto &mem : m_non_recursive_member_structs) {
				outer_while->body->add_as_stmt(std::make_unique<NodeFunctionCall>(
					false,
					std::make_unique<NodeFunctionHeaderRef>(
						mem->ty->get_element_type()->to_string() + OBJ_DELIMITER + "__decr__",
						std::make_unique<NodeParamList>(mem->tok, to_member_chain_ref(mem), std::make_unique<NodeInt>(1, mem->tok)),
						mem->tok
					),
					mem->tok
				));
				// nil_check->if_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
				// 	to_member_chain_ref(mem),
				// 	std::make_unique<NodeNil>(mem->tok),
				// 	mem->tok
				// ));
				// outer_while->body->add_as_stmt(std::move(nil_check));
				wrap_in_loop(outer_while->body->statements.back(), mem, iter_decl->variable);

			}
			for(auto & mem : m_recursive_member_structs) {
				// current.next
				auto mem_ref = to_member_chain_ref(mem, m_self_ref.get());
				// current := self.next
				outer_while->body->add_as_stmt(
					std::make_unique<NodeSingleAssignment>(
					clone_as<NodeReference>(current_ref.get()),
					mem_ref->clone(),
					mem->tok
				));
			}

		}
		// add actual delete function
		outer_while->body->add_as_stmt(std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				m_struct.name + OBJ_DELIMITER + "__del__",
				std::make_unique<NodeParamList>(tok, m_self_ref->clone()),
				tok
			),
			tok
		));

		m_decr_func->body->add_as_stmt(std::move(outer_while));
		m_decr_func->parent = &m_struct;
		m_decr_func->ty = TypeRegistry::Void;
		m_decr_func->header->create_function_type(TypeRegistry::Void);
		m_decr_func->collect_references();
		return std::move(m_decr_func);
	}


private:

	std::unique_ptr<NodeFunctionDefinition> get_base_func(const std::string& name, std::unique_ptr<NodeDataStructure> add_param = nullptr) {
		auto func_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				name,
				std::make_unique<NodeFunctionParam>(clone_as<NodeDataStructure>(m_struct.node_self.get())),
				tok
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		if(add_param) {
			func_def->header->add_param(std::move(add_param));
		}
		return func_def;
	}

	std::unique_ptr<NodeReference> to_member_chain_ref(std::shared_ptr<NodeDataStructure> mem, NodeReference* idx = nullptr) {
		std::unique_ptr<NodeReference> ref;
		// if composite -> get raw array (if ndarray) and set index to iterator
		if(auto node_comp = cast_node<NodeComposite>(mem.get())) {
			auto raw_array = node_comp->get_raw()->to_reference();
			raw_array->cast<NodeArrayRef>()->set_index(m_iterator_ref->clone());
			ref = std::move(raw_array);
		} else {
			ref = mem->to_reference();
		}
		ref->remove_obj_prefix();
		ref->ty = mem->ty->get_element_type();
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
			auto member = mem.second.lock();
			if(mem.first == "self") continue;
			if(member->is_engine) continue;
			if(member->data_type ==DataType::Const) continue;
			if(member->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
				auto mem_type = member->ty->get_element_type();
				if(recursive_structs.find(mem_type->to_string()) != recursive_structs.end()) {
					m_recursive_member_structs.push_back(member);
				} else {
					m_non_recursive_member_structs.push_back(member);
				}
			}
		}
	}

	/// gets pointer to node_self from function definition and sets the declaration of m_self_ref
	void set_self_ref_declaration(NodeFunctionDefinition* func_def) {
		m_self_ref->declaration = func_def->header->get_param(0);
	}

	/// gets pointer to num_refs from function definition and sets the declaration of m_num_refs_ref
	void set_num_refs_ref_declaration(NodeFunctionDefinition* func_def) {
		m_num_refs_ref->declaration = func_def->header->get_param(1);
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
	///	else
	///		List::allocation[self] := 0
	///	end if
	std::unique_ptr<NodeIf> alloc_array_check_continue(std::unique_ptr<NodeAST> idx =nullptr) {
		auto alloc_ref = clone_as<NodeArrayRef>(m_alloc_ref.get());
		if(idx) {
			alloc_ref->set_index(std::move(idx));
		}
		auto node_if = std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(
				token::GREATER_THAN,
				alloc_ref->clone(),
				std::make_unique<NodeInt>(0, tok),
				tok
			),
			std::make_unique<NodeBlock>(tok, true),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		node_if->if_body->add_as_stmt(DefinitionProvider::continu(tok));
		node_if->else_body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			std::move(alloc_ref),
			std::make_unique<NodeInt>(0, tok),
			tok
		));
		return node_if;
	}

};