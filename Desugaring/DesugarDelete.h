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
		for(auto &ref : node.ptrs) {
			if(const auto var_ref = ref->cast<NodeVariableRef>()) {
				ref->replace_with(var_ref->to_pointer_ref());
			}
		}
		auto block = std::make_unique<NodeBlock>(node.tok);
		for(auto &ptr : node.ptrs) {
			block->add_as_stmt(
				std::make_unique<NodeSingleDelete>(
					std::move(ptr),
					std::make_unique<NodeInt>(1, node.tok),
					node.tok)
				);
		}
		return node.replace_with(std::move(block));
	}

};
