//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once

#include "Tokenizer.h"
#include "AST.h"
#include "Tokens.h"


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

	[[nodiscard]] std::optional<Token> peek(int ahead = 0) const;
	Token consume(int tokens = 1);
    static int get_binop_precedence(token tok);

    std::optional<NodeInt> parse_int();
    std::optional<NodeVariable> parse_variable();
    std::optional<NodeAST> parse_binary_expr();
    std::optional<NodeAST> parse_binary_expr_rhs(int precedence, std::optional<NodeAST> lhs);
    /// ( expression )
    std::optional<NodeAST> parse_parenth_expr();
    /// parse identifierexpr, numberexpr, parenthexpr
    std::optional<NodeAST> parse_primary_expr();
    std::optional<NodeVariableAssign> parse_variable_assign();
//    std::optional<NodeAssignStatement> parse_assign_statement();
//    std::optional<NodeStatements> parse_statements();
//    std::optional<NodeCallback> parse_callback();
};

