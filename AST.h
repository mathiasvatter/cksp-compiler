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
	inline NodeVariable(std::string name, char ident) : name(std::move(name)), ident(ident) {}
	void accept(ASTVisitor& visitor) override;

};

struct NodeBinaryExpr: NodeAST {
    numberType type; //real or int?
	NodeAST left, right;
	std::string op;

    inline explicit NodeBinaryExpr(std::string op, NodeAST left, NodeAST right)
    : op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeVariableAssign: NodeAST {
    NodeVariable variable;
    std::string assignment_op;
    std::variant<NodeAST, NodeBinaryExpr> assignee;
    char linebreak;

    inline NodeVariableAssign(NodeVariable variable,std::string assignmentOp,
                       std::variant<NodeAST, NodeBinaryExpr> assignee, char linebreak) : variable(
            std::move(variable)), assignment_op(std::move(assignmentOp)), assignee(std::move(assignee)), linebreak(linebreak) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeAssignStatement: NodeAST {
    NodeVariableAssign assignment;

	explicit NodeAssignStatement(NodeVariableAssign assignment) : assignment(std::move(assignment)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeStatements: NodeAST {
    std::vector<NodeAssignStatement> statements;

	explicit NodeStatements(std::vector<NodeAssignStatement> statements) : statements(std::move(statements)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeCallback: NodeAST {
    std::string begin_callback;
    char linebreak;
    std::vector<NodeStatements> statements;
    std::string end_callback;

	NodeCallback(std::string begin_callback,
				 char linebreak,
				 std::vector<NodeStatements> statements,
				 std::string end_callback)
		: begin_callback(std::move(begin_callback)), linebreak(linebreak), statements(std::move(statements)), end_callback(std::move(end_callback)) {}
	void accept(ASTVisitor& visitor) override;
};




