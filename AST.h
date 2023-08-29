//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <variant>

#include "Tokens.h"

enum numberType {
    INTEGER,
    REAL
};
//class ASTVisitor;

struct NodeAST {
    NodeAST() = default;
	virtual ~NodeAST() = default;
	virtual void accept(class ASTVisitor& visitor);
};


struct NodeInt : NodeAST {
	int value;
	inline explicit NodeInt(int v) : value(v) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeVariable: NodeAST {
    char ident;
	std::string name;
	inline NodeVariable(const std::string &name, char ident) : name(name), ident(ident) {}
	void accept(ASTVisitor& visitor) override;

};

struct NodeBinaryExpr: NodeAST {
    numberType type; //real or int?
	std::unique_ptr<NodeAST> left, right;
	std::string op;

    inline explicit NodeBinaryExpr(std::string op, std::unique_ptr<NodeAST> left, std::unique_ptr<NodeAST> right)
    : op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeVariableAssign: NodeAST {
    std::unique_ptr<NodeVariable> variable;
    std::string assignment_op;
    std::unique_ptr<NodeAST> assignee;
    char linebreak;

    inline NodeVariableAssign(std::unique_ptr<NodeVariable> variable,std::string assignmentOp,
                       std::unique_ptr<NodeAST> assignee, char linebreak) : variable(
            std::move(variable)), assignment_op(std::move(assignmentOp)), assignee(std::move(assignee)), linebreak(linebreak) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeAssignStatement: NodeAST {
    std::unique_ptr<NodeVariableAssign> assignment;

	explicit NodeAssignStatement(std::unique_ptr<NodeVariableAssign> assignment) : assignment(std::move(assignment)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeStatements: NodeAST {
    std::vector<std::unique_ptr<NodeAST>> statements;

	explicit NodeStatements(std::vector<std::unique_ptr<NodeAST>> statements) : statements(std::move(statements)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeCallback: NodeAST {
    std::string begin_callback;
    char linebreak;
    std::unique_ptr<NodeStatements> statements;
    std::string end_callback;

	inline NodeCallback(const std::string &begin_callback,
				 char linebreak,
				 std::unique_ptr<NodeStatements> statements,
				 const std::string &end_callback)
		: begin_callback(begin_callback), linebreak(linebreak), statements(std::move(statements)), end_callback(end_callback) {}
	void accept(ASTVisitor& visitor) override;
};




