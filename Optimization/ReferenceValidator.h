//
// Created by Mathias Vatter on 10.06.25.
//

#pragma once


#include "../AST/ASTVisitor/ASTNoVisitor.h"

/**
 * This class returns a pointer to a NodeReference if the node is a subclass of NodeReference
 */
class ReferenceValidator final : public ASTNoVisitor {
	NodeReference* reference = nullptr;

public:
	NodeReference* cast_reference(NodeAST &node) {
		reference = nullptr;
		node.accept(*this);
		return reference;
	}


private:
	NodeAST* visit(NodeVariableRef& node) override {
		reference = &node;
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		reference = &node;
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		reference = &node;
		return &node;
	}

	NodeAST* visit(NodeFunctionHeaderRef& node) override {
		reference = &node;
		return &node;
	}

	NodeAST* visit(NodeListRef& node) override {
		reference = &node;
		return &node;
	}

	NodeAST* visit(NodePointerRef& node) override {
		reference = &node;
		return &node;
	}

	NodeAST* visit(NodeAccessChain& node) override {
		reference = &node;
		return &node;
	}

};