//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once
#include <string>

struct ASTNode {
	virtual ~ASTNode() = default;
};

struct IntNode : ASTNode {
	int value;
	inline IntNode(int v) : value(v) {}
};

struct VariableNode: ASTNode {
	std::string name;
	VariableNode(const std::string& n) : name(n) {}
};

struct BinaryOpNode: ASTNode {
	std::string op;
	BinaryOpNode(const std::string& op) : op(op) {}
};

struct ExpressionNode: ASTNode {
	std::shared_ptr<ASTNode> left;
	std::shared_ptr<ASTNode> right;

	BinaryOpNode
};
