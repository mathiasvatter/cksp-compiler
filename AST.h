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
	ParamList,
    Void

};

enum VarType {
    Const,
    Polyphonic,
    Array,
    Mutable
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

struct NodeVariable: NodeAST {
    char ident;
    VarType var_type = VarType::Mutable;
	std::string name;
	inline NodeVariable(std::string name, VarType type, char ident)
    : name(std::move(name)), var_type(type), ident(ident) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeParamList: NodeAST {
    std::vector<std::unique_ptr<NodeAST>> params;
    inline explicit NodeParamList(std::vector<std::unique_ptr<NodeAST>> params={}) : params(std::move(params)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeArray : NodeAST {
    std::unique_ptr<NodeVariable> name;
    std::unique_ptr<NodeParamList> sizes = nullptr;
    std::unique_ptr<NodeParamList> indexes;
    inline NodeArray(std::unique_ptr<NodeVariable> name, std::unique_ptr<NodeParamList> sizes,
              std::unique_ptr<NodeParamList> indexes)
              : name(std::move(name)), sizes(std::move(sizes)), indexes(std::move(indexes)) {}
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

struct NodeDeclareStatement : NodeAST {
	std::unique_ptr<NodeParamList> to_be_declared;
	std::unique_ptr<NodeAST> assignee;
	inline explicit NodeDeclareStatement(std::unique_ptr<NodeParamList> to_be_declared, std::unique_ptr<NodeAST> assignee)
		: to_be_declared(std::move(to_be_declared)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeAssignStatement: NodeAST {
    std::unique_ptr<NodeParamList> array_variable;
    std::unique_ptr<NodeAST> assignee;
    inline NodeAssignStatement(std::unique_ptr<NodeParamList> array_variable, std::unique_ptr<NodeAST> assignee)
    : array_variable(std::move(array_variable)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
};

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeAST {
    std::unique_ptr<NodeAST> statement;
	explicit NodeStatement(std::unique_ptr<NodeAST> statement) : statement(std::move(statement)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeIfStatement: NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    std::vector<std::unique_ptr<NodeStatement>> else_statements = {};
    inline NodeIfStatement(std::unique_ptr<NodeAST> condition, std::vector<std::unique_ptr<NodeStatement>> statements,std::vector<std::unique_ptr<NodeStatement>> elseStatements)
    : condition(std::move(condition)), statements(std::move(statements)), else_statements(std::move(elseStatements)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeForStatement : NodeAST {
    std::unique_ptr<NodeAST> iterator;
    Token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline NodeForStatement(std::unique_ptr<NodeAST> iterator, Token to, std::unique_ptr<NodeAST> iterator_end, std::vector<std::unique_ptr<NodeStatement>> statements)
    : iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeWhileStatement : NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline NodeWhileStatement(std::unique_ptr<NodeAST> condition, std::vector<std::unique_ptr<NodeStatement>> statements)
    : condition(std::move(condition)), statements(std::move(statements)) {}
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
    std::unique_ptr<NodeAST> args;

    inline NodeFunctionHeader(std::string name, std::unique_ptr<NodeAST> args)
    : name(std::move(name)), args(std::move(args)) {};
    void accept(ASTVisitor& visitor) override;
};

struct NodeFunctionCall : NodeAST {
    bool is_call=false;
    std::unique_ptr<NodeFunctionHeader> function;
    inline NodeFunctionCall(bool isCall, std::unique_ptr<NodeFunctionHeader> function)
    : is_call(isCall), function(std::move(function)) {}
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




