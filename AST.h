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

struct NodeAST {
    NodeAST() = default;
	virtual ~NodeAST() = default;
    virtual inline void print() const {};
};

struct NodeInt : NodeAST {
	int value;
	inline explicit NodeInt(int v) : value(v) {}
    inline void print() const override {
        std::cout << "IntNode: " << value << std::endl;
    }
};

struct NodeVariable: NodeAST {
    char ident;
	std::string name;
	inline NodeVariable(std::string name, char ident) : name(std::move(name)), ident(ident) {}
    inline void print() const override {
        std::cout << "VariableNode: " << name << " (" << ident << ")" << std::endl;
    }
};

struct NodeBinaryExpr: NodeAST {
    numberType type; //real or int?
	std::variant<NodeVariable, NodeInt, NodeAST> left, right;
	std::string op;

    inline explicit NodeBinaryExpr(std::string op, std::variant<NodeVariable, NodeInt, NodeAST> left, std::variant<NodeVariable, NodeInt, NodeAST> right)
    : op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
    inline void print() const override {
        std::cout << "BinaryExprNode: " << op << std::endl;
        std::visit([&](auto& node) { node.print(); }, left);
        std::visit([&](auto& node) { node.print(); }, right);
    }
};

struct NodeVariableAssign: NodeAST {
    NodeVariable variable;
    std::string assignment_op;
    std::variant<NodeAST> assignee;
    char linebreak;

    inline NodeVariableAssign(NodeVariable variable,std::string assignmentOp,
                       std::variant<NodeAST> assignee) : variable(
            std::move(variable)), assignment_op(std::move(assignmentOp)), assignee(std::move(assignee)) {}
    inline void print() const override {
        std::cout << "VariableAssignment: " << assignment_op << std::endl;
        std::visit([&](auto& node) { node.print(); }, assignee);
    }
};

struct NodeAssignStatement: NodeAST {
    NodeVariableAssign assignment;
};

struct NodeStatements: NodeAST {
    std::vector<NodeAssignStatement> statements;
//    explicit NodeStatements(std::vector<std::unique_ptr<NodeAST>> statements)
//    : statements(std::move(statements)) {}
};

struct NodeCallback: NodeAST {
    std::string begin_callback;
    char linebreak;
    std::vector<NodeStatements> statements;
    std::string end_callback;
//    explicit NodeCallback(std::string begin_cb, std::vector<std::unique_ptr<NodeAST>> stmts,
//                          std::string end_cb)
//    : begin_callback(std::move(begin_cb)), statements(std::move(stmts)), end_callback(std::move(end_cb)) {}
};