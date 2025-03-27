//
// Created by Mathias Vatter on 21.05.24.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Deflates thread-unsafe Parameters after Parameter Promotion by replacing MAX_CALLBACKS in
 * Array references with NI_CALLBACK_ID mod MAX_CALLBACKS.
 */
class DeflateParameters final : public ASTVisitor {
	DefinitionProvider* m_def_provider;

public:
	explicit DeflateParameters(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

private:
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);

		if (node.is_builtin_kind()) return &node;

		if(const auto definition = node.get_definition()) {
			// only visit definition if not already visited
			if (!definition->visited) {
				definition->accept(*this);
				definition->visited = true;
			}
		}

		return &node;
	}


};
