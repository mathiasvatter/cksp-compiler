//
// Created by Mathias Vatter on 01.09.26.
//

#pragma once

#include "../ASTVisitor/ASTVisitor.h"

/**
 * Collects every function call inside a given node. Stays within the node: a call is recorded, but
 * the definition it points at is not descended into.
 */
class FunctionCallCollector final : public ASTVisitor {
	std::vector<NodeFunctionCall*> m_calls;

public:
	const std::vector<NodeFunctionCall*>& collect(NodeAST& node) {
		m_calls.clear();
		node.accept(*this);
		return m_calls;
	}

	/// releases the collected nodes so that a static collector does not keep the AST alive
	void clear() {
		m_calls.clear();
		m_calls.shrink_to_fit();
	}

private:
	NodeAST* visit(NodeFunctionCall& node) override {
		m_calls.push_back(&node);
		node.function->accept(*this);
		return &node;
	}
};
