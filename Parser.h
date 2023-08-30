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
        {token::BOOL_OR, 1},
        {token::BOOL_AND, 2},
        {token::BOOL_NOT, 3},
        {token::GREATER_THAN, 4},
        {token::LESS_THAN, 5},
        {token::GREATER_EQUAL, 6},
        {token::LESS_EQUAL, 7},
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

class Parser {

public:
    explicit Parser(std::vector<Token> tokens);

private:
	size_t m_pos;
	std::vector<Token> m_tokens;
	token curr_token;

	[[nodiscard]] Token peek(int ahead = 0);
	Token consume();
    static int get_binop_precedence(token tok);

    std::optional<std::unique_ptr<NodeInt>> parse_int();
    std::optional<std::unique_ptr<NodeVariable>> parse_variable();
    std::optional<std::unique_ptr<NodeAST>> parse_binary_expr();
    std::optional<std::unique_ptr<NodeAST>> _parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs);
    /// ( expression )
    std::optional<std::unique_ptr<NodeAST>> _parse_parenth_expr();
    /// parse identifierexpr, numberexpr, parenthexpr
    std::optional<std::unique_ptr<NodeAST>> _parse_primary_expr();
    std::optional<std::unique_ptr<NodeVariableAssign>> parse_variable_assign();
    std::optional<std::unique_ptr<NodeAST>> parse_assign_statement();
    std::optional<std::unique_ptr<NodeStatements>> parse_statements();
    Result<std::unique_ptr<NodeCallback>> parse_callback();
};

