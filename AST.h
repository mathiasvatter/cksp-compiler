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
#include "Tokenizer.h"

enum ASTType {
    Integer,
    Real,
    Boolean,
    Comparison,
    String,
    Unknown,
    Void

};

struct NodeAST {
    ASTType type;
    inline NodeAST() : type(ASTType::Unknown) {}
	virtual ~NodeAST() = default;
	virtual void accept(class ASTVisitor& visitor);
};

struct NodeInt : NodeAST {
	int value;
	inline explicit NodeInt(int v) : value(v) {type = ASTType::Integer;}
	void accept(ASTVisitor& visitor) override;
};

struct NodeReal : NodeAST {
    float value;
    inline explicit NodeReal(float value) : value(value) {type = ASTType::Real;}
    void accept(ASTVisitor& visitor) override;
};

struct NodeString : NodeAST {
    std::string value;
    inline explicit NodeString(std::string value) : value(std::move(value)) {type = ASTType::String;}
    void accept(ASTVisitor& visitor) override;
};

struct NodeVariableDeclaration {
    
};

struct NodeVariable: NodeAST {
    char ident;
	std::string name;
	inline NodeVariable(std::string name, char ident) : name(std::move(name)), ident(ident) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeArray : NodeAST {
    std::unique_ptr<NodeVariable> name;
    std::unique_ptr<NodeAST> size = nullptr;
    std::vector<std::unique_ptr<NodeAST>> indexes;
    inline NodeArray(std::unique_ptr<NodeVariable> name, std::unique_ptr<NodeAST> size,
              std::vector<std::unique_ptr<NodeAST>> indexes)
              : name(std::move(name)), size(std::move(size)), indexes(std::move(indexes)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeUnaryExpr : NodeAST {
    std::unique_ptr<NodeAST> operand;
    Token op;
    inline NodeUnaryExpr(Token op, std::unique_ptr<NodeAST> operand) : operand(std::move(operand)), op(std::move(op)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeBinaryExpr: NodeAST {
	std::unique_ptr<NodeAST> left, right;
	std::string op;
    inline explicit NodeBinaryExpr(std::string op, std::unique_ptr<NodeAST> left, std::unique_ptr<NodeAST> right)
    : op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeAssignStatement: NodeAST {
    std::unique_ptr<NodeAST> array_variable;
    std::unique_ptr<NodeAST> assignee;
    inline NodeAssignStatement(std::unique_ptr<NodeAST> array_variable, std::unique_ptr<NodeAST> assignee)
    : array_variable(std::move(array_variable)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
};

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeAST {
    std::unique_ptr<NodeAST> statement;
	explicit NodeStatement(std::unique_ptr<NodeAST> statement) : statement(std::move(statement)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeCallback: NodeAST {
    std::string begin_callback;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    std::string end_callback;

	inline NodeCallback(std::string begin_callback,
				 std::vector<std::unique_ptr<NodeStatement>> statements,
				 std::string end_callback)
		: begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeImport : NodeAST {
    std::string filepath;
    std::string alias;

    inline explicit NodeImport(std::string filepath, std::string alias="") : filepath(std::move(filepath)), alias(std::move(alias)) {}
};

struct NodeFunctionHeader: NodeAST {
    std::string name;
    std::vector<std::unique_ptr<NodeAST>> args;

    inline NodeFunctionHeader(std::string name, std::vector<std::unique_ptr<NodeAST>> args)
    : name(std::move(name)), args(std::move(args)) {};
    void accept(ASTVisitor& visitor) override;
};

struct NodeFunctionDefinition: NodeAST {
    std::unique_ptr<NodeFunctionHeader> header;
    std::optional<std::unique_ptr<NodeVariable>> return_variable;
    bool override;
    std::vector<std::unique_ptr<NodeStatement>> body;

    inline NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
                           std::optional<std::unique_ptr<NodeVariable>> returnVariable, bool override,
                           std::vector<std::unique_ptr<NodeStatement>> body)
                           : header(std::move(header)), return_variable(std::move(returnVariable)), override(override),
                           body(std::move(body)) {};
    void accept(ASTVisitor& visitor) override;
};

struct NodeProgram: NodeAST {
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
    std::vector<std::unique_ptr<NodeAST>> macro_definitions;
    std::vector<std::unique_ptr<NodeImport>> imports;
    // TODO macro ASTNode still in progress

    inline NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
					   std::vector<std::unique_ptr<NodeFunctionDefinition>> functionDefinitions,
                       std::vector<std::unique_ptr<NodeImport>> imports,
					   std::vector<std::unique_ptr<NodeAST>> macroDefinitions)
                 : callbacks(std::move(callbacks)), function_definitions(std::move(functionDefinitions)),
                   imports(std::move(imports)), macro_definitions(std::move(macroDefinitions)) {}
    void accept(ASTVisitor& visitor) override;
};




