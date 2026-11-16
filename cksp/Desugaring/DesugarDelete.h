//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Checks if all reference in Delete Statement are PointerRef, if not -> transform to PointerRef
 * Desugaring Delete Statements with multiple PointerRefs into SingleDelete Nodes
 */
class DesugarDelete final : public ASTDesugaring {
public:
	explicit DesugarDelete(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST* visit(NodeDelete& node) override {
		auto block = std::make_unique<NodeBlock>(node.tok);
		for(auto &ptr : node.ptrs) {
			auto del = std::make_unique<NodeSingleDelete>(std::move(ptr), std::make_unique<NodeInt>(1, node.tok), node.tok);
			del->destroy_instance = true;
			block->add_as_stmt(std::move(del));
		}
		return node.replace_with(std::move(block));
	}

};
