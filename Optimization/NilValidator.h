//
// Created by Mathias Vatter on 03.11.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * This class returns true if node is nil
 */
class NilValidator : public ASTVisitor {
private:
	bool m_is_nil = false;

public:
	bool is_nil(NodeAST &node) {
		m_is_nil = false;
		node.accept(*this);
		return m_is_nil;
	}

	NodeAST* visit(NodeNil& node) override {
		if(node.is_func_arg()) return &node;

		m_is_nil = true;

		return &node;
	}

};