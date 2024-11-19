//
// Created by Mathias Vatter on 17.06.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringStruct : public ASTLowering {
private:
	NodeFunctionDefinition* m_current_func = nullptr;
	NodeStruct* m_current_struct = nullptr;
	std::unique_ptr<NodeVariableRef> m_max_structs_ref = std::make_unique<NodeVariableRef>("MAX_STRUCTS", Token());
	inline bool in_constructor() {
		return m_current_func and m_current_struct and m_current_func == m_current_struct->constructor.get();
	}
	/// returns free_idx as reference if in constructor, self as reference if not
	inline std::unique_ptr<NodeReference> get_index_ref() {
		if(in_constructor()) {
			return m_current_struct->free_idx_var->to_reference();
		}
		return m_current_struct->node_self->to_reference();
	}
public:
	explicit LoweringStruct(NodeProgram *program) : ASTLowering(program) {}


	NodeAST * visit(NodeStruct& node) override {
		m_current_struct = &node;

		node.members->accept(*this);
		m_current_struct = &node;
		for(auto & m: node.methods) {
			m->accept(*this);
		}

		m_current_struct = nullptr;
		m_current_func = nullptr;
//		node.rebuild_member_table();
		return &node;
	}

	inline NodeAST * visit(NodeAccessChain& node) override {
		return node.lower(m_program);
	}

	inline NodeAST * visit(NodeSingleDeclaration& node) override {
		// "self" gets deleted in the inline_struct method
		if(node.variable == m_current_struct->node_self) {
			return &node;
		}
		// turn member into array if it is a member
		node.variable->accept(*this);
		if(node.variable->is_member()) {
			node.variable->is_local = false;
		}
		if(node.value) node.value->accept(*this);

		// turn node.value into param list if is member and has been turned into array
		if(node.variable->is_member() and node.value) {
			if(node.variable->get_node_type() != NodeType::Array and node.variable->get_node_type() != NodeType::NDArray) {
				return &node;
			}
			if(node.value->get_node_type() != NodeType::InitializerList) {
				node.value = std::make_unique<NodeInitializerList>(node.tok, std::move(node.value));
				node.value->parent = &node;
			}
		}
		return &node;
	}

	inline NodeAST * visit(NodeVariable& node) override {
		// if member, turn into array
		if(node.is_member() and !node.is_engine) {
			auto new_node = node.inflate_dimension(m_current_struct->max_individual_struts_var->to_reference());
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}
	inline NodeAST * visit(NodePointer& node) override {
		// if member, turn into array of pointers
		if(node.is_member() and !node.is_engine) {
			auto new_node = node.inflate_dimension(m_current_struct->max_individual_struts_var->to_reference());
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}
	inline NodeAST * visit(NodeArray& node) override {
		// if member, turn into multi-dimensional array
		/*
		 * 	declare velocities[10]: [int]
		 * 	declare struct.velocity[struct.MAX_STRUCTS, 10]
		 */
		if(node.is_member() and !node.is_engine) {
			auto new_node = node.inflate_dimension(m_current_struct->max_individual_struts_var->to_reference());
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}
	inline NodeAST * visit(NodeNDArray& node) override {
		// if member, turn into multi-dimensional array
		if(node.is_member() and !node.is_engine) {
			auto new_node = node.inflate_dimension(m_current_struct->max_individual_struts_var->to_reference());
			new_node->is_local = false;
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}

	inline NodeAST * visit(NodeVariableRef& node) override {
		// if member reference, turn into array reference with (struct.free_idx as index if in constructor, self as index if not)
		if(node.is_member_ref() and !node.is_engine) {
			auto new_node = node.inflate_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}
	inline NodeAST * visit(NodeArrayRef& node) override {
		// if member reference, turn into multi-dimensional array reference with struct.free_idx as index
		if(node.is_member_ref() and !node.is_engine) {
			auto new_node = node.inflate_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}

	inline NodeAST * visit(NodePointerRef& node) override {
		if(node.is_member_ref() and !node.is_engine) {
			auto new_node = node.inflate_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}

	inline NodeAST * visit(NodeNDArrayRef& node) override {
		// if member reference, turn into multi-dimensional array reference with struct.free_idx as index
		if(node.is_member_ref() and !node.is_engine) {
			auto new_node = node.inflate_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}


	inline NodeAST * visit(NodeFunctionDefinition& node) override {
		// no lowering necessary for delete methods
//		static const std::unordered_set<std::string> destructors = {
//			OBJ_DELIMITER+"__del__",
//			OBJ_DELIMITER+"__decr__",
//			OBJ_DELIMITER+"__incr__"
//		};
//		for (const auto& destructor : destructors) {
//			if (node.header->name.find(destructor) != std::string::npos) {
//				return &node;
//			}
//		}
		m_current_func = &node;
		node.header->accept(*this);
		node.body->accept(*this);
		// lower init function
		if(node.header->name == m_current_struct->name+OBJ_DELIMITER+"__init__") {
			lower_init_method(&node);
		}
		return &node;
	}

private:
	/**
	 * Lower init function by adding:
	 *  struct.free_idx := search(struct.allocation, 0)
	 * 	if struct.free_idx = -1
	 * 		message("Error: No more free space available to allocate objects of type 'note'")
	 * 	end if
	 * 	struct.allocation[struct.free_idx] := 1
	 * 	...
	 * 	return struct.free_idx
	 */
	inline void lower_init_method(NodeFunctionDefinition* init) {
		auto node_block = std::make_unique<NodeBlock>(init->tok);
		auto node_search_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"search",
				std::make_unique<NodeParamList>(
					init->tok,
					m_current_struct->allocation_var->to_reference(),
					std::make_unique<NodeInt>(0, init->tok)
				),
				init->tok
			),
			init->tok
		);
		node_search_call->kind = NodeFunctionCall::Kind::Builtin;
		auto node_assign_search = std::make_unique<NodeSingleAssignment>(
			m_current_struct->free_idx_var->to_reference(),
			std::move(node_search_call),
			init->tok
		);
		node_block->add_as_stmt(std::move(node_assign_search));

		auto node_error_message = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"message",
				std::make_unique<NodeParamList>(
					init->tok,
					std::make_unique<NodeString>(
						"\"Error: No more free space available to allocate objects of type '"+ m_current_struct->name + "'\"",
						init->tok)
				),
				init->tok
			),
			init->tok
		);
		node_error_message->kind = NodeFunctionCall::Kind::Builtin;
		auto node_if_stmt = std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(token::EQUAL,
											 m_current_struct->free_idx_var->to_reference(),
											 std::make_unique<NodeInt>(-1, init->tok),
											 init->tok),
			std::make_unique<NodeBlock>(init->tok,
										std::make_unique<NodeStatement>(std::move(node_error_message), init->tok)),
			std::make_unique<NodeBlock>(init->tok),
			init->tok
		);
		node_if_stmt->if_body->scope = true;
		node_block->add_as_stmt(std::move(node_if_stmt));

		auto node_allocation = m_current_struct->allocation_var->to_reference();
		static_cast<NodeArrayRef *>(node_allocation.get())->index = m_current_struct->free_idx_var->to_reference();
		auto node_assign_allocation = std::make_unique<NodeSingleAssignment>(
			std::move(node_allocation),
			std::make_unique<NodeInt>(1, init->tok),
			init->tok
		);
		node_block->add_as_stmt(std::move(node_assign_allocation));
		init->body->prepend_body(std::move(node_block));
		init->body->add_as_stmt(
			std::make_unique<NodeReturn>(init->tok, m_current_struct->free_idx_var->to_reference())
		);
	}


};