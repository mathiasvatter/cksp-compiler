//
// Created by Mathias Vatter on 17.06.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringStruct final : public ASTLowering {
	NodeFunctionDefinition* m_current_func = nullptr;
	NodeStruct* m_current_struct = nullptr;
	std::unique_ptr<NodeVariableRef> m_max_structs_ref = std::make_unique<NodeVariableRef>("MAX::STRUCTS", Token());
	bool in_constructor() const {
		return m_current_func and m_current_struct and m_current_func == m_current_struct->constructor.get();
	}
	/// returns free_idx as reference if in constructor, self as reference if not
	std::unique_ptr<NodeReference> get_index_ref() const {
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
		for(const auto & m: node.methods) {
			m->accept(*this);
		}

		m_current_struct = nullptr;
		m_current_func = nullptr;
		return &node;
	}

	NodeAST * visit(NodeAccessChain& node) override {
		return node.lower(m_program);
	}

	NodeAST * visit(NodeSingleDeclaration& node) override {
		// "self" gets deleted in the struct method -> ignore here
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
			if(!node.variable->cast<NodeArray>() and !node.variable->cast<NodeNDArray>()) {
				return &node;
			}
			if(!node.value->cast<NodeInitializerList>()) {
				node.set_value(std::make_unique<NodeInitializerList>(node.tok, std::move(node.value)));
			}
		}
		return &node;
	}

	NodeAST * visit(NodeVariable& node) override {
		// if member, turn into array
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(m_current_struct->max_individual_structs_var->to_reference());
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}
	NodeAST * visit(NodePointer& node) override {
		// if member, turn into array of pointers
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(m_current_struct->max_individual_structs_var->to_reference());
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}
	NodeAST * visit(NodeArray& node) override {
		// if member, turn into multi-dimensional array
		/*
		 * 	declare velocities[10]: [int]
		 * 	declare struct.velocity[struct.MAX_STRUCTS, 10]
		 */
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(m_current_struct->max_individual_structs_var->to_reference());
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}
	NodeAST * visit(NodeNDArray& node) override {
		// if member, turn into multi-dimensional array
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(m_current_struct->max_individual_structs_var->to_reference());
			new_node->is_local = false;
			return node.replace_datastruct(std::move(new_node));
		}
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		// if member reference, turn into array reference with (struct.free_idx as index if in constructor, self as index if not)
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}
	NodeAST * visit(NodeArrayRef& node) override {
		// if member reference, turn into multi-dimensional array reference with struct.free_idx as index
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}

	NodeAST * visit(NodePointerRef& node) override {
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}

	NodeAST * visit(NodeNDArrayRef& node) override {
		// if member reference, turn into multi-dimensional array reference with struct.free_idx as index
		if(determine_inflation_need(node)) {
			auto new_node = node.expand_dimension(get_index_ref());
			return node.replace_reference(std::move(new_node));
		}
		return &node;
	}


	NodeAST * visit(NodeFunctionDefinition& node) override {
		m_current_func = &node;
		node.header->accept(*this);
		node.body->accept(*this);
		// lower init function
		if(node.header->name == m_current_struct->name+OBJ_DELIMITER+"__init__") {
			node.mark_threadsafety(m_program);
			if (!node.is_thread_safe) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message = "Constructor of struct <"+m_current_struct->name+"> contains non-threadsafe builtin commands"
									" which can corrupt value consistency within it. "
									"Do not use commands like <wait> or <wait_async> in constructors.";
				error.exit();
			}
			lower_init_method(&node);
		}
		return &node;
	}

private:
	static bool determine_inflation_need(const NodeReference& ref) {
		return ref.is_member_ref() and !ref.is_engine and ref.get_declaration()->data_type != DataType::Const;
	}

	static bool determine_inflation_need(const NodeDataStructure& data) {
		return data.is_member() and !data.is_engine and data.data_type != DataType::Const;
	}

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
	void lower_init_method(NodeFunctionDefinition* init) const {
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