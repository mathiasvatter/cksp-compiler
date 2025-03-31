//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Transform Get Control nodes into set control nodes when l_values in assignment
 */
class DesugarSingleAssignment final : public ASTDesugaring {
public:
	explicit DesugarSingleAssignment(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST* visit(NodeSingleAssignment& node) override {

		if(!node.l_value->cast<NodeGetControl>()) {
			return &node;
		}

		auto node_get_control = static_cast<NodeGetControl*>(node.l_value.get());
		auto type = node_get_control->get_control_type();
		auto set_control = std::make_unique<NodeSetControl>(
			std::move(node_get_control->ui_id),
			node_get_control->control_param,
			std::move(node.r_value),
			node.tok
		);
		set_control->ty = type;

		return node.replace_with(std::move(set_control));
	}

};
