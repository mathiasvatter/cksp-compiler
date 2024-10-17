//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Checks if all reference in Delete Statement are PointerRef, if not -> transform to PointerRef
 * Desugaring Delete Statements with multiple PointerRefs into SingleDelete Nodes
 */
class DesugarDelete : public ASTDesugaring {
public:
	explicit DesugarDelete(NodeProgram* program) : ASTDesugaring(program) {};

	inline NodeAST* visit(NodeDelete& node) override {
		for(auto &ref : node.ptrs) {
			if(ref->get_node_type() == NodeType::VariableRef) {
				auto var_ref = static_cast<NodeVariableRef*>(ref.get());
				ref->replace_with(var_ref->to_pointer_ref());
			}
		}
		auto block = std::make_unique<NodeBlock>(node.tok);
		for(auto &ptr : node.ptrs) {
			block->add_stmt(std::make_unique<NodeStatement>(std::make_unique<NodeSingleDelete>(std::move(ptr), node.tok), node.tok));
		}
		return node.replace_with(std::move(block));
	}

};
