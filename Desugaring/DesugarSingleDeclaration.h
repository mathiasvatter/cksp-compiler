//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * If declared variable is an array or ndarray, check if l_value is initializer list
 */
class DesugarSingleDeclaration final : public ASTDesugaring {
public:
	explicit DesugarSingleDeclaration(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (node.value) {
			if (node.variable->cast<NodeArray>() or node.variable->cast<NodeNDArray>()) {
				if (!node.value->cast<NodeInitializerList>()) {
					auto init_list = std::make_unique<NodeInitializerList>(node.tok, std::move(node.value));
					node.set_value(std::move(init_list));
				}
			}
		}
		return &node;
	}

};
