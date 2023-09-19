//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once

#include "Tokenizer.h"
#include "AST.h"
#include "Tokens.h"
#include "Result.h"
//#include "ASTVisitor.h"

inline static std::map<token, int> BinaryOpPrecendence = {
		{token::COMMA, 1},
        {token::BOOL_OR, 1},
        {token::BOOL_AND, 2},
		{token::BOOL_NOT, 3},
//		{token::BOOL, 3},
        {token::GREATER_THAN, 4},
        {token::LESS_THAN, 5},
        {token::GREATER_EQUAL, 6},
        {token::LESS_EQUAL, 7},
		{token::EQUAL, 7},
		{token::COMPARISON, 7},
        {token::BIT_XOR, 8},
        {token::BIT_OR, 9},
        {token::BIT_AND, 10},
        {token::BIT_NOT, 11},
        {token::ADD, 12},
        {token::SUB, 12},
        {token::MULT, 13},
        {token::DIV, 13},
        {token::MODULO, 13}
};

inline static bool is_unary_operator(token op) {
    return op == token::SUB || op == token::BIT_NOT || op == token::BOOL_NOT;
}

class Parser {

public:
    explicit Parser(std::vector<Token> tokens);

private:
	size_t m_pos;
	std::vector<Token> m_tokens;
	token curr_token;

	[[nodiscard]] Token peek(int ahead = 0);
	Token consume();
    static int _get_binop_precedence(token tok);
	void _skip_linebreaks();

    Result<std::unique_ptr<NodeAST>> parse_number();
    Result<std::unique_ptr<NodeString>> parse_string();
    Result<std::unique_ptr<NodeVariable>> parse_variable();
	/// handles the expression inside brackets as size if is_size=true, else those are handled as indexes
    Result<std::unique_ptr<NodeArray>> parse_array(std::unique_ptr<NodeVariable> array_variable);
	/// stops either at end token or at linebreak
    Result<std::unique_ptr<NodeParamList>> parse_param_list();
	Result<std::unique_ptr<NodeAST>> _parse_into_param_list(std::unique_ptr<NodeAST> expression);
    /// parses every expression from binary, string, unary to number and variable
    Result<std::unique_ptr<NodeAST>> parse_expression();
    Result<std::unique_ptr<NodeAST>> parse_string_expr();
        /// Helper function for parsing binary string expression recursively
        Result<std::unique_ptr<NodeAST>> _parse_string_expr_rhs(std::unique_ptr<NodeAST> lhs);
    /// parse unary or binary expression
    Result<std::unique_ptr<NodeAST>> parse_binary_expr();
    Result<std::unique_ptr<NodeAST>> parse_unary_expr();
	    /// Helper function for parsing binary expressions recursion
		Result<std::unique_ptr<NodeAST>> _parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs);
		/// ( expression )
		Result<std::unique_ptr<NodeAST>> _parse_parenth_expr();
		/// parse identifierexpr, numberexpr, parenthexpr, functionheader
		Result<std::unique_ptr<NodeAST>> _parse_primary_expr();
    Result<std::unique_ptr<NodeAST>> parse_assign_statement();
	Result<std::unique_ptr<NodeAST>> parse_declare_statement();
	Result<std::unique_ptr<NodeDefineStatement>> parse_define_statement();
	/// combines all possible statement types
    Result<std::unique_ptr<NodeStatement>> parse_statement();
    Result<std::unique_ptr<NodeIfStatement>> parse_if_statement();
    Result<std::unique_ptr<NodeForStatement>> parse_for_statement();
    Result<std::unique_ptr<NodeWhileStatement>> parse_while_statement();
	Result<std::unique_ptr<NodeSelectStatement>> parse_select_statement();
    Result<std::unique_ptr<NodeFunctionDefinition>> parse_function_definition();
    Result<std::unique_ptr<NodeFunctionHeader>> parse_function_header();
    Result<std::unique_ptr<NodeFunctionCall>> parse_function_call();
    Result<std::unique_ptr<NodeCallback>> parse_callback();
    Result<std::unique_ptr<NodeImport>> parse_import();
	Result<std::unique_ptr<NodeProgram>> parse_program();

};

