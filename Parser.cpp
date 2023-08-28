//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"
#include "ASTVisitor.h"

#include <utility>

Parser::Parser(std::vector<Token> tokens): m_tokens(std::move(tokens)) {
	m_pos = 0;
    while(peek()->type == token::COMMENT || peek()->type == token::LINEBRK)
        consume();
	ASTPrinter printer;
	parse_assign_statement().value().accept(printer);
//    parse_variable_assign().value();

}

std::optional<Token> Parser::peek(int ahead) const {
	if (m_tokens.size() < m_pos+ahead)
		return {};
	return m_tokens.at(m_pos+ahead);

}

Token Parser::consume(int tokens) {
    if (m_pos < m_tokens.size())
	    return m_tokens.at(m_pos++);
    exit(EXIT_FAILURE);
}

std::optional<NodeInt> Parser::parse_int() {
    if (peek().has_value()) {
        if (peek()->type == token::INT) {
            return NodeInt(std::stoi(consume().val));
        } else if (peek()->type == token::SUB && peek(1)->type == token::INT) {
            auto value = consume().val;
            value += consume().val;
            return NodeInt(std::stoi(value));
        }
    }
    return {};
}

std::optional<NodeVariable> Parser::parse_variable() {
    if (peek().has_value() && peek()->type == token::KEYWORD) {
        // see if variable already has identifier
        char ident = 0;
        std::string var_name = peek()->val;
        if (contains(VAR_IDENT, peek()->val[0])) {
            ident = peek()->val[0];
            var_name = peek()->val.substr(1);
        }
        consume();
        return NodeVariable(var_name, ident);
    }
    return {};
}

std::optional<NodeAST> Parser::parse_binary_expr() {
    if (peek().has_value()) {
        if(auto lhs = _parse_primary_expr()) {
            return _parse_binary_expr_rhs(0, lhs);
        }
    }
    return {};
}

std::optional<NodeAST> Parser::_parse_binary_expr_rhs(int precedence, std::optional<NodeAST> lhs) {
    while(true) {
        int prec = get_binop_precedence(peek()->type);
        if(prec < precedence)
            return lhs;
        // its not -1 so it is a binop
        auto bin_op = peek()->val;
        consume();
        auto rhs = _parse_primary_expr();
        if (!rhs.has_value()) {
            return {};
        }
        int next_precedence = get_binop_precedence(peek()->type);
        if (prec < next_precedence) {
            rhs = _parse_binary_expr_rhs(prec + 1, rhs);
            if (!rhs.has_value()) {
                return {};
            }
        }
        lhs = NodeBinaryExpr(bin_op, lhs.value(), rhs.value());
    }
}

std::optional<NodeAST> Parser::_parse_parenth_expr() {
    if (peek().has_value()) {
        consume(); // eat (
        auto V = parse_binary_expr();
        if (peek()->type != token::CLOSED_PARENTH) {
            std::cerr << "Expected ')' in lin " << peek()->line << std::endl;
            return {};
        }
        consume(); // eat )
        return V;
    }

    return {};
}


std::optional<NodeAST> Parser::_parse_primary_expr() {
    if (peek().has_value()) {
        if (peek()->type == token::KEYWORD) {
            return parse_variable();
        } else if (peek()->type == token::INT) {
            return parse_int();
        } else if (peek()->type == token::OPEN_PARENTH) {
            return _parse_parenth_expr();
        } else {
            std::cerr << "Unknown expression token!" << std::endl;
        }
    }
    return {};
}

int Parser::get_binop_precedence(token tok) {
    int precedence = BinaryOpPrecendence[tok];
    if (precedence <= 0) {
//        std::cerr << precedence << " is not a valid expression token." << std::endl;
        return -1;
    }
    return precedence;
}

std::optional<NodeVariableAssign> Parser::parse_variable_assign() {
	auto variable = parse_variable();
	auto assignment = peek()->val;
	consume();
	if (peek().has_value()) {
		auto expr = parse_binary_expr();
		if (peek().has_value() && peek()->type == token::LINEBRK) {
			auto linebreak = consume().val[0];
			return NodeVariableAssign(variable.value(), assignment, expr.value(), linebreak);
		}
		else
			std::cerr << "Expected linebreak in line "<< peek()->line << std::endl;
	}
    return {};
}

std::optional<NodeAssignStatement> Parser::parse_assign_statement() {
	if (peek().has_value() && peek()->type == token::KEYWORD) {
		if (peek(1).has_value() && peek(1)->type == token::ASSIGN) {
			return NodeAssignStatement(parse_variable_assign().value());
		} else {
			std::cerr << "Expected assignment operator in line "<< peek()->line << std::endl;
		}
	}
	return {};
}

