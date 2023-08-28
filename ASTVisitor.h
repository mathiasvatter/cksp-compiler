//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include <iostream>
#include "AST.h"

class ASTVisitor {
public:
	virtual void visit(NodeInt& node) = 0;
	virtual void visit(NodeVariable& node) = 0;
	virtual void visit(NodeBinaryExpr& node) = 0;
	virtual void visit(NodeVariableAssign& node)  = 0;
	virtual void visit(NodeAssignStatement& node)  = 0;
	virtual void visit(NodeStatements& node)  = 0;
	virtual void visit(NodeCallback& node)  = 0;

};

class ASTPrinter : public ASTVisitor {
public:
	void visit(NodeInt& node) override {
		std::cout << node.value << std::endl;
	}

	void visit(NodeVariable& node) override {
		std::cout << node.name;
		if (node.ident) {
			std::cout << " (" << node.ident << ")";
		}
		std::cout << std::endl;
	}

	void visit(NodeBinaryExpr& node) override {
		std::cout << "BinaryExpr(";
		node.left.accept(*this);
		std::cout << ", " << node.op << ", ";
		node.right.accept(*this);
		std::cout << ")" << std::endl;
	}

	void visit(NodeVariableAssign& node) override {
		std::cout << "Assigning to variable: ";
		node.variable.accept(*this);
		std::cout << "Operation: " << node.assignment_op << std::endl;
		std::cout << "Assignee: ";
		std::get<NodeAST>(node.assignee).accept(*this);
	}

	void visit(NodeAssignStatement& node) override {
		std::cout << "Assignment Statement: " << std::endl;
		node.assignment.accept(*this);
	}

	void visit(NodeStatements& node) override {
		std::cout << "Statements: " << std::endl;
		for(auto& statement : node.statements) {
			statement.accept(*this);
		}
	}

	void visit(NodeCallback& node) override {
		std::cout << "Callback: " << node.begin_callback << std::endl;
		for(auto& statement : node.statements) {
			statement.accept(*this);
		}
		std::cout << "End Callback: " << node.end_callback << std::endl;
	}
};

