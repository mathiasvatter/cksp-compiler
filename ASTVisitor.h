//
// Created by Mathias Vatter on 28.08.23.
//

#pragma once

#include <iostream>
#include "AST.h"

class ASTVisitor {
public:
	virtual void visit(NodeInt& node) = 0;
    virtual void visit(NodeReal& node) = 0;
    virtual void visit(NodeString& node) = 0;
    virtual void visit(NodeVariable& node) = 0;
    virtual void visit(NodeArray& node) = 0;
    virtual void visit(NodeBinaryExpr& node) = 0;
    virtual void visit(NodeUnaryExpr& node) = 0;
	virtual void visit(NodeStringExpr& node) = 0;
	virtual void visit(NodeVariableAssign& node)  = 0;
	virtual void visit(NodeAssignStatement& node)  = 0;
	virtual void visit(NodeStatement& node)  = 0;
	virtual void visit(NodeCallback& node)  = 0;
    virtual void visit(NodeProgram& node)  = 0;
    virtual void visit(NodeFunctionHeader& node)  = 0;
    virtual void visit(NodeFunctionDefinition& node)  = 0;
};

class ASTPrinter : public ASTVisitor {
public:
	void visit(NodeInt& node) override {
		std::cout << node.value;
	}

    void visit(NodeReal& node) override {
        std::cout << node.value;
    }

    void visit(NodeString& node) override {
        std::cout << node.value;
    }

	void visit(NodeVariable& node) override {
		std::cout << "(" << node.ident << ")" << node.name;
	}

    void visit(NodeArray& node) override {
        std::cout << "(" << node.ident << ")" << node.name << "[" << node.size << "].at(" << node.idx << ")";
    }

	void visit(NodeBinaryExpr& node) override {
        std::string expression_type = "BinaryExpr(";
        if(node.type == Comparison)
            expression_type = "ComparisonExpr(";
        else if (node.type == Boolean)
            expression_type = "BooleanExpr(";
        else if (node.type ==String)
            expression_type = "StringExpr(";
		std::cout << expression_type;
		node.left->accept(*this);
		std::cout << " " << node.op << " ";
		node.right->accept(*this);
		std::cout << ")" ;
	}

    void visit(NodeUnaryExpr& node) override {
        std::cout << "UnaryExpr(";
        std::cout << node.op.val << " ";
        node.operand->accept(*this);
        std::cout << ")" ;
    }

	void visit(NodeStringExpr& node) override {
		std::cout << "StringExp(";
        for(auto& expr : node.string_expressions) {
            expr->accept(*this);
            std::cout << ", ";
        }
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
		std::cout << ")";
	}

	void visit(NodeStatement& node) override {
		std::cout << "StmtWrapper(" ;
		node.statement->accept(*this);
		std::cout << ")" << std::endl;
	}

	void visit(NodeCallback& node) override {
		std::cout << "Callback(" << node.begin_callback << ")" << std::endl;
		std::cout << "Stmts{" << std::endl ;
		for(auto& statement : node.statements) {
			statement->accept(*this);
		}
		std::cout << "}" << std::endl;
		std::cout << "End_callback(" << node.end_callback << ")"<< std::endl;
	}

    void visit(NodeFunctionHeader& node) override {
        std::cout << node.name << "(";
        for(auto& arg: node.args) {
            arg->accept(*this);
			std::cout << ", ";
        }
        std::cout << ")";
    }
    void visit(NodeFunctionDefinition& node) override {
        std::cout << "function ";
        node.header ->accept(*this);
		std::cout << "\n";
        if (node.return_variable.has_value())
            node.return_variable.value()->accept(*this);
        if (node.override) {
            std::cout << "override" << std::endl;
        }
        for(auto& stmt: node.body) {
            stmt->accept(*this);
        }
        std::cout << "end function" << std::endl;
    }
    void visit(NodeProgram& node) override {
        std::cout << "Callbacks: " << std::endl;
        for(auto& callback: node.callbacks) {
            callback->accept(*this);
        }
        std::cout << "Functions:" << std::endl;
        for( auto & function : node.function_definitions) {
            function->accept(*this);
        }
        std::cout << "Macros:" << std::endl;
        for (auto & macro : node.macro_definitions) {
            macro -> accept(*this);
        }
    }

};

