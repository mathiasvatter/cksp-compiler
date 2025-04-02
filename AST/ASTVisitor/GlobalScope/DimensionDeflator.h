//
// Created by Mathias Vatter on 02.04.25.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Goal is to deflate expanded arrays (that are lowered now) by knowing the dimension value to deflate
 * type_str00[MAX::CB_STACK]: int[] -> type_str00[1]: int[] -> type_str00: int
 */
class DimensionDeflator final : public ASTVisitor {

public:
	explicit DimensionDeflator(NodeProgram* main) {
		m_program = main;
	}

	/// takes only thread-unsafe nodearrays
	NodeAST* convert_to_thread_safe(NodeDataStructure& node) {
		if (node.is_thread_safe) return &node;
		node.accept(*this);
		return &node;
	}

private:
	NodeAST* visit(NodeArray& node) override {
		if (node.size) {
			node.size->do_constant_folding();
			node.size->accept(*this);
			if (const auto node_int = node.size->cast<NodeInt>()) {
				if (node_int->value == 1) {
					auto scalar = node.to_variable();
					scalar->ty = node.ty->get_element_type();
					return node.replace_datastruct(std::move(scalar));
				}
			}
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if (m_program->max_cb_stack->name == node.name) {
			node.replace_with(std::make_unique<NodeInt>(1, node.tok));
		}
		return &node;
	}

};