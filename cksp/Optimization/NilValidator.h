//
// Created by Mathias Vatter on 03.11.24.
//

#pragma once

#include <algorithm>

#include "../ASTVisitor/ASTNoVisitor.h"

/**
 * This class returns true if node can only ever evaluate to nil: the literal nil, a ternary whose
 * branches are both nil, or an initializer list of nothing but nil. A nil anywhere else inside the
 * node does not count - <c ? find() : nil> is not nil, so it does not descend into other nodes.
 */
class NilValidator final : public ASTNoVisitor {
private:
	bool m_is_nil = false;

public:
	bool is_nil(NodeAST &node) {
		m_is_nil = false;
		node.accept(*this);
		return m_is_nil;
	}

	NodeAST* visit(NodeNil& node) override {
		m_is_nil = true;
		return &node;
	}

	NodeAST* visit(NodeTernary& node) override {
		m_is_nil = is_nil(*node.if_branch) and is_nil(*node.else_branch);
		return &node;
	}

	NodeAST* visit(NodeInitializerList& node) override {
		m_is_nil = std::ranges::all_of(node.elements, [this](const auto& element) { return is_nil(*element); });
		return &node;
	}

};
