//
// Created by Mathias Vatter on 12.12.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * Tries to transform initializer lists to range nodes when applicable.
 * As for now: only init lists in for-each loop ranges will be transformed.
 */
class LoweringInitializerList final : public ASTLowering {
	DefinitionProvider* m_def_provider;
public:
	explicit LoweringInitializerList(NodeProgram *program) : ASTLowering(program) {
		m_def_provider = program->def_provider;
	}

	NodeAST * visit(NodeInitializerList &node) override {
		node.flatten();
		if(node.parent->cast<NodeForEach>() or node.parent->parent->cast<NodeForEach>()) {
			if(auto range = node.transform_to_range()) {
				return node.replace_with(std::move(range.value()));
			}
		}
		return &node;
	}

};
