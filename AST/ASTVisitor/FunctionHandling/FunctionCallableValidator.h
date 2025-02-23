//
// Created by Mathias Vatter on 23.02.25.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Validates if given function call is callable. This includes:
 *	- call is not in init callback
 *	- call definition has no restricted commands
 *	@input: NodeFunctionCall that is user-defined, current callback pointer, function call needs to be dfs-visited
 */
class FunctionCallableValidator final : public ASTVisitor {
	NodeCallback* m_current_callback = nullptr;
	bool m_is_callable = true;
public:
	explicit FunctionCallableValidator(NodeProgram *main) {
		m_program = main;
	}

	bool is_callable(NodeFunctionCall& call, NodeCallback* current_callback) {
		m_current_callback = current_callback;
		if (m_program->is_init_callback(m_current_callback)) {
			return false;
		}
		call.accept(*this);
		return m_is_callable;
	}


private:

	NodeAST* visit(NodeFunctionCall &node) override {
		node.bind_definition(m_program);
		const auto definition = node.get_definition();
		if (!definition) {
			m_is_callable = false;
			return &node;
		}

		if (node.kind == NodeFunctionCall::Kind::Builtin) {
			m_is_callable &= !definition->is_restricted;
		}

		definition->accept(*this);

		return &node;
	}

};