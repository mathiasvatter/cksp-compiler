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
		std::cout << node.value;
	}

	void visit(NodeVariable& node) override {
		std::cout << node.name << "(" << node.ident << ") ";
	}

	void visit(NodeBinaryExpr& node) override {
		std::cout << "BinaryExpr(";
		std::cout << node.op << " ";
		node.left->accept(*this);
		std::cout << " ";
		node.right->accept(*this);
		std::cout << ")" ;
	}

	void visit(NodeVariableAssign& node) override {
		std::cout << "VariableAssign(";
		node.variable->accept(*this);
		std::cout << node.assignment_op << " ";
		node.assignee->accept(*this);
		std::cout << ")";
	}

	void visit(NodeAssignStatement& node) override {
		std::cout << "AssignStmt(";
		node.assignment->accept(*this);
		std::cout << ") " << std::endl;
	}

	void visit(NodeStatements& node) override {
		std::cout << "Stmts {" << std::endl;
		for(auto& statement : node.statements) {
			statement->accept(*this);
		}
		std::cout << "}" << std::endl;
	}

	void visit(NodeCallback& node) override {
		std::cout << "Begin_Callback(" << node.begin_callback << ")" << std::endl;
		node.statements->accept(*this);
		std::cout << "End_Callback(" << node.end_callback << ")"<< std::endl;
	}
};

