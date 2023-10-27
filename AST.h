//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <optional>
#include <variant>
#include <list>


#include "Tokenizer/Tokens.h"
#include "Tokenizer/Tokenizer.h"

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
    Mutable,
	Define,
    UI_Control
};

struct NodeAST {
    Token tok;
    ASTType type;
    NodeAST* parent = nullptr;
    inline explicit NodeAST(const Token tok=Token()) : tok(tok), type(ASTType::Unknown) {}
	virtual ~NodeAST() = default;
	virtual void accept(class ASTVisitor& visitor);
	virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) {};
	void replace_with(std::unique_ptr<NodeAST> newNode);
};

struct NodeInt : NodeAST {
	int32_t value;
	inline explicit NodeInt(int32_t v, const Token tok) : NodeAST(tok), value(v) {type = ASTType::Integer;}
	void accept(ASTVisitor& visitor) override;
};

struct NodeReal : NodeAST {
    double value;
    inline explicit NodeReal(double value, Token tok) : NodeAST(tok), value(value) {type = ASTType::Real;}
    void accept(ASTVisitor& visitor) override;
};

struct NodeString : NodeAST {
    std::string value;
    inline explicit NodeString(std::string value, Token tok) : NodeAST(tok), value(std::move(value)) {type = ASTType::String;}
    void accept(ASTVisitor& visitor) override;
};

struct NodeVariable: NodeAST {
    bool is_persistent;
    bool is_local;
    VarType var_type = VarType::Mutable;
	std::string name;
	inline NodeVariable(bool is_persistent, std::string name, VarType type, Token tok) : NodeAST(tok), is_persistent(is_persistent), name(std::move(name)), var_type(type) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeParamList: NodeAST {
    std::vector<std::unique_ptr<NodeAST>> params;
    inline explicit NodeParamList(std::vector<std::unique_ptr<NodeAST>> params, Token tok) : NodeAST(tok), params(std::move(params)) {}
    void accept(ASTVisitor& visitor) override;
	virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
};

struct NodeArray : NodeAST {
    bool is_persistent;
    bool is_local;
    VarType var_type = VarType::Array;
    std::string name;
    std::unique_ptr<NodeParamList> sizes = nullptr;
    std::unique_ptr<NodeParamList> indexes;
    inline explicit NodeArray(Token tok) : NodeAST(tok) {}
    inline NodeArray(bool is_persistent, std::string name, VarType var_type, std::unique_ptr<NodeParamList> sizes,
              std::unique_ptr<NodeParamList> indexes, Token tok)
              : NodeAST(tok), is_persistent(is_persistent), name(std::move(name)), var_type(var_type),
              sizes(std::move(sizes)), indexes(std::move(indexes)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeUIControl : NodeAST {
    std::string ui_control_type;
    std::unique_ptr<NodeAST> control_var; //Array or Variable
    std::unique_ptr<NodeParamList> params;
    inline explicit NodeUIControl(Token tok) : NodeAST(tok) {}
    inline NodeUIControl(std::string uiControlType, std::unique_ptr<NodeAST> controlVar, std::unique_ptr<NodeParamList> params, Token tok)
                : NodeAST(tok), ui_control_type(std::move(uiControlType)), control_var(std::move(controlVar)), params(std::move(params)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeUnaryExpr : NodeAST {
    Token op;
    std::unique_ptr<NodeAST> operand;
    inline explicit NodeUnaryExpr(Token tok) : NodeAST(tok) {}
    inline NodeUnaryExpr(Token op, std::unique_ptr<NodeAST> operand, Token tok) : NodeAST(tok), operand(std::move(operand)), op(std::move(op)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeBinaryExpr: NodeAST {
	std::unique_ptr<NodeAST> left, right;
	std::string op;
    inline explicit NodeBinaryExpr(Token tok) : NodeAST(tok) {}
    inline NodeBinaryExpr(std::string op, std::unique_ptr<NodeAST> left, std::unique_ptr<NodeAST> right, Token tok)
    : NodeAST(tok), op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
	void accept(ASTVisitor& visitor) override;
	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
};

struct NodeDeclareStatement : NodeAST {
	std::unique_ptr<NodeParamList> to_be_declared;
	std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeDeclareStatement(Token tok) : NodeAST(tok) {}
	inline NodeDeclareStatement(std::unique_ptr<NodeParamList> to_be_declared, std::unique_ptr<NodeParamList> assignee, Token tok)
    : NodeAST(tok), to_be_declared(std::move(to_be_declared)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
//	void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
};

struct NodeAssignStatement: NodeAST {
    std::unique_ptr<NodeParamList> array_variable;
    std::unique_ptr<NodeParamList> assignee;
    inline explicit NodeAssignStatement(Token tok) : NodeAST(tok) {}
    inline NodeAssignStatement(std::unique_ptr<NodeParamList> array_variable, std::unique_ptr<NodeParamList> assignee, Token tok)
    : NodeAST(tok), array_variable(std::move(array_variable)), assignee(std::move(assignee)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeGetControlStatement : NodeAST {
    std::unique_ptr<NodeAST> ui_id; //array or variable
    std::string control_param;
    inline NodeGetControlStatement(std::unique_ptr<NodeAST> uiId, std::string controlParam, Token tok)
    : NodeAST(tok), ui_id(std::move(uiId)), control_param(std::move(controlParam)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeSetControlStatement : NodeAST {
    std::unique_ptr<NodeGetControlStatement> get_control;
    std::unique_ptr<NodeAST> assignee;
    inline NodeSetControlStatement(std::unique_ptr<NodeGetControlStatement> getControl, std::unique_ptr<NodeAST> assignee, Token tok)
    : NodeAST(tok), get_control(std::move(getControl)), assignee(std::move(assignee)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeConstStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeDeclareStatement>> constants;
    inline explicit NodeConstStatement(Token tok) : NodeAST(tok) {}
    inline NodeConstStatement(std::string prefix, std::vector<std::unique_ptr<NodeDeclareStatement>> constants, Token tok)
    : NodeAST(tok), prefix(std::move(prefix)), constants(std::move(constants)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeStructStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeDeclareStatement>> members;
    inline explicit NodeStructStatement(Token tok) : NodeAST(tok) {}
    inline NodeStructStatement(std::string prefix, std::vector<std::unique_ptr<NodeDeclareStatement>> members, Token tok)
    : NodeAST(tok), prefix(std::move(prefix)), members(std::move(members)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeFamilyStatement : NodeAST {
    std::string prefix;
    std::vector<std::unique_ptr<NodeDeclareStatement>> members;
    inline explicit NodeFamilyStatement(Token tok) : NodeAST(tok) {}
    inline NodeFamilyStatement(std::string prefix, std::vector<std::unique_ptr<NodeDeclareStatement>> members, Token tok)
    : NodeAST(tok), prefix(std::move(prefix)), members(std::move(members)) {}
    void accept(ASTVisitor& visitor) override;
};

// can be assign_statement, if_statement etc.
struct NodeStatement: NodeAST {
    std::unique_ptr<NodeAST> statement;
    inline explicit NodeStatement(Token tok) : NodeAST(tok) {}
    inline NodeStatement(std::unique_ptr<NodeAST> statement, Token tok) : NodeAST(tok), statement(std::move(statement)) {}
	void accept(ASTVisitor& visitor) override;
	virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
};

struct NodeIfStatement: NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    std::vector<std::unique_ptr<NodeStatement>> else_statements = {};
    inline explicit NodeIfStatement(Token tok) : NodeAST(tok) {}
    inline NodeIfStatement(std::unique_ptr<NodeAST> condition, std::vector<std::unique_ptr<NodeStatement>> statements,std::vector<std::unique_ptr<NodeStatement>> elseStatements, Token tok)
    : NodeAST(tok), condition(std::move(condition)), statements(std::move(statements)), else_statements(std::move(elseStatements)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeForStatement : NodeAST {
    std::unique_ptr<NodeAssignStatement> iterator;
    Token to;
    std::unique_ptr<NodeAST> iterator_end;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline explicit NodeForStatement(Token tok) : NodeAST(tok) {}
    inline NodeForStatement(std::unique_ptr<NodeAssignStatement> iterator, Token to, std::unique_ptr<NodeAST> iterator_end, std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
    : NodeAST(tok), iterator(std::move(iterator)), to(std::move(to)), iterator_end(std::move(iterator_end)), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
	virtual void replace_child(NodeAST* oldChild, std::unique_ptr<NodeAST> newChild) override;
};

struct NodeWhileStatement : NodeAST {
    std::unique_ptr<NodeAST> condition;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    inline explicit NodeWhileStatement(Token tok) : NodeAST(tok) {}
    inline NodeWhileStatement(std::unique_ptr<NodeAST> condition, std::vector<std::unique_ptr<NodeStatement>> statements, Token tok)
    : NodeAST(tok), condition(std::move(condition)), statements(std::move(statements)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeSelectStatement : NodeAST {
	std::unique_ptr<NodeAST> expression;
	std::map<std::unique_ptr<NodeAST>, std::vector<std::unique_ptr<NodeStatement>>> cases;
    inline explicit NodeSelectStatement(Token tok) : NodeAST(tok) {}
	inline NodeSelectStatement(std::unique_ptr<NodeAST> expression, std::map<std::unique_ptr<NodeAST>, std::vector<std::unique_ptr<NodeStatement>>> cases, Token tok)
    : NodeAST(tok), expression(std::move(expression)), cases(std::move(cases)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeCallback: NodeAST {
    std::string begin_callback;
    std::vector<std::unique_ptr<NodeStatement>> statements;
    std::string end_callback;
    inline explicit NodeCallback(Token tok) : NodeAST(tok) {}
	inline NodeCallback(std::string begin_callback, std::vector<std::unique_ptr<NodeStatement>> statements, std::string end_callback, Token tok)
     : NodeAST(tok), begin_callback(std::move(begin_callback)), statements(std::move(statements)), end_callback(std::move(end_callback)) {}
	void accept(ASTVisitor& visitor) override;
};

struct NodeImport : NodeAST {
    std::string filepath;
    std::string alias;
    inline explicit NodeImport(std::string filepath, std::string alias, Token tok)
    : NodeAST(tok), filepath(std::move(filepath)), alias(std::move(alias)) {}
};

struct NodeFunctionHeader: NodeAST {
    std::string name;
    std::unique_ptr<NodeParamList> args;
    inline explicit NodeFunctionHeader(Token tok) : NodeAST(tok) {}
    inline NodeFunctionHeader(std::string name, std::unique_ptr<NodeParamList> args, Token tok)
    : NodeAST(tok), name(std::move(name)), args(std::move(args)) {};
    void accept(ASTVisitor& visitor) override;
};

struct NodeFunctionCall : NodeAST {
    bool is_call=false;
    std::unique_ptr<NodeFunctionHeader> function;
    inline explicit NodeFunctionCall(Token tok) : NodeAST(tok) {}
    inline NodeFunctionCall(bool isCall, std::unique_ptr<NodeFunctionHeader> function, Token tok)
    : NodeAST(tok), is_call(isCall), function(std::move(function)) {}
    void accept(ASTVisitor& visitor) override;
};

struct NodeFunctionDefinition: NodeAST {
    std::unique_ptr<NodeFunctionHeader> header;
    std::optional<std::unique_ptr<NodeVariable>> return_variable;
    bool override;
    std::vector<std::unique_ptr<NodeStatement>> body;
    inline explicit NodeFunctionDefinition(Token tok) : NodeAST(tok) {}
    inline NodeFunctionDefinition(std::unique_ptr<NodeFunctionHeader> header,
	   std::optional<std::unique_ptr<NodeVariable>> returnVariable, bool override,
	   std::vector<std::unique_ptr<NodeStatement>> body, Token tok)
	   : NodeAST(tok), header(std::move(header)), return_variable(std::move(returnVariable)), override(override),
	   body(std::move(body)) {};
    void accept(ASTVisitor& visitor) override;
};

struct NodeDefineHeader : NodeAST {
	std::string name;
	std::vector<std::vector<Token>> args;
	inline NodeDefineHeader(std::string name, std::vector<std::vector<Token>> args, Token tok)
	: NodeAST(tok), name(std::move(name)), args(std::move(args)) {}
};

struct NodeDefineStatement : NodeAST {
	std::unique_ptr<NodeDefineHeader> to_be_defined;
	std::vector<Token> assignee;
    bool has_recursive_calls;
	inline NodeDefineStatement(std::unique_ptr<NodeDefineHeader> to_be_defined, std::vector<Token> assignee, bool recur)
		: to_be_defined(std::move(to_be_defined)), assignee(std::move(assignee)), has_recursive_calls(recur) {}
};

struct NodeMacroHeader : NodeAST {
    std::string name;
    std::vector<std::vector<Token>> args;
	size_t token_pos;
    inline NodeMacroHeader(std::string name, std::vector<std::vector<Token>> args, size_t pos, Token tok)
    : NodeAST(tok), name(std::move(name)), args(std::move(args)), token_pos(pos) {}
};

struct NodeMacroDefinition : NodeAST {
	std::unique_ptr<NodeMacroHeader> header;
    std::vector<Token> body;
    bool has_recursive_calls;
    inline NodeMacroDefinition(std::unique_ptr<NodeMacroHeader> header, std::vector<Token> body, bool recur, Token tok)
    : NodeAST(tok), header(std::move(header)), body(std::move(body)), has_recursive_calls(recur) {}
};

struct NodeProgram: NodeAST {
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
    std::vector<std::unique_ptr<NodeMacroDefinition>> macro_definitions;
    std::vector<std::unique_ptr<NodeImport>> imports;
	std::vector<std::unique_ptr<NodeDefineStatement>> defines;
    // TODO macro ASTNode still in progress
    inline explicit NodeProgram(Token tok) : NodeAST(tok) {}
    inline NodeProgram(std::vector<std::unique_ptr<NodeCallback>> callbacks,
					   std::vector<std::unique_ptr<NodeFunctionDefinition>> functionDefinitions,
                       std::vector<std::unique_ptr<NodeImport>> imports,
					   std::vector<std::unique_ptr<NodeMacroDefinition>> macroDefinitions,
					   std::vector<std::unique_ptr<NodeDefineStatement>> defines, Token tok)
                       : NodeAST(tok), callbacks(std::move(callbacks)), function_definitions(std::move(functionDefinitions)),
                   imports(std::move(imports)), macro_definitions(std::move(macroDefinitions)), defines(std::move(defines)) {}
    void accept(ASTVisitor& visitor) override;
};




