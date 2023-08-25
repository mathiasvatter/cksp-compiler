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

struct ExpressionNode: ASTNode {
	std::unique_ptr<ASTNode> left, right;
	std::string op;
	ExpressionNode(std::string op, std::unique_ptr<ASTNode> left, std::unique_ptr<ASTNode> right)
	: op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
};
