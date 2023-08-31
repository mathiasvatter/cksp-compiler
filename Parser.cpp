//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"
#include "ASTVisitor.h"

#include <utility>

Parser::Parser(std::vector<Token> tokens): m_tokens(std::move(tokens)) {
	m_pos = 0;
    while(peek().type == token::COMMENT || peek().type == token::LINEBRK)
        consume();
	ASTPrinter printer;
    auto callback = parse_callback();
    if (callback.is_error())
        callback.get_error().print();
    else
	    callback.unwrap()->accept(printer);
//    parse_variable_assign().value();

}

Token Parser::peek(int ahead) {
	if (m_tokens.size() < m_pos+ahead) {
        auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
        CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val).print();
        exit(EXIT_FAILURE);
    }
//	curr_token = m_tokens.at(m_pos+ahead).type;
	return m_tokens.at(m_pos+ahead);

}

Token Parser::consume() {
    if (m_pos < m_tokens.size()) {
//        curr_token = m_tokens.at(m_pos + 1).type;
        return m_tokens.at(m_pos++);
    }
    auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
    CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val).print();
    exit(EXIT_FAILURE);
}

std::optional<std::unique_ptr<NodeInt>> Parser::parse_int() {
    if (peek().type == token::INT) {
        return std::make_unique<NodeInt>(std::stoi(consume().val));
    } else if (peek().type == token::SUB && peek(1).type == token::INT) {
        auto value = consume().val;
        value += consume().val;
        return std::make_unique<NodeInt>(std::stoi(value));
    } else {
        auto err_msg = "Found unknown Integer Syntax.";
        CompileError(ErrorType::ParseError, err_msg, peek().line, "(negative) Integer", peek().val).print();
        exit(EXIT_FAILURE);
    }
}

std::optional<std::unique_ptr<NodeVariable>> Parser::parse_variable() {
    if (peek().type == token::KEYWORD) {
        // see if variable already has identifier
        char ident = 0;
        std::string var_name = peek().val;
        if (contains(VAR_IDENT, peek().val[0])) {
            ident = peek().val[0];
            var_name = peek().val.substr(1);
        }
        consume();
        return std::make_unique<NodeVariable>(var_name, ident);
    }
    auto err_msg = "Found unknown Variable Syntax.";
    CompileError(ErrorType::ParseError, err_msg, peek().line, "valid Variable", peek().val).print();
    exit(EXIT_FAILURE);
}

std::optional<std::unique_ptr<NodeAST>> Parser::parse_binary_expr() {
    if(auto lhs = _parse_primary_expr()) {
        return _parse_binary_expr_rhs(0, std::move(lhs.value()));
    }
    auto err_msg = "Found unknown Binary Expression Syntax.";
    CompileError(ErrorType::ParseError, err_msg, peek().line, "valid Expression", peek().val).print();
    exit(EXIT_FAILURE);
}

std::optional<std::unique_ptr<NodeAST>> Parser::_parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs) {
    while(true) {
        int prec = get_binop_precedence(peek().type);
        if(prec < precedence)
            return lhs;
        // its not -1 so it is a binop
        auto bin_op = peek().val;
        consume();
        auto rhs = _parse_primary_expr();
        if (!rhs.has_value()) {
            return {};
        }
        int next_precedence = get_binop_precedence(peek().type);
        if (prec < next_precedence) {
            rhs = _parse_binary_expr_rhs(prec + 1, std::move(rhs.value()));
            if (!rhs.has_value()) {
                return {};
            }
        }
        lhs = std::make_unique<NodeBinaryExpr>(bin_op, std::move(lhs), std::move(rhs.value()));
    }
}

std::optional<std::unique_ptr<NodeAST>> Parser::_parse_parenth_expr() {
    consume(); // eat (
    auto V = parse_binary_expr();
    if (peek().type != token::CLOSED_PARENTH) {
        auto err_msg = "Missing parenthesis.";
        CompileError(ErrorType::ParseError, err_msg, peek().line, ")", peek().val).print();
        exit(EXIT_FAILURE);
    }
    consume(); // eat )
    return V;
}


std::optional<std::unique_ptr<NodeAST>> Parser::_parse_primary_expr() {
    if (peek().type == token::KEYWORD) {
        return parse_variable();
    } else if (peek().type == token::INT) {
        return parse_int();
    } else if (peek().type == token::OPEN_PARENTH) {
        return _parse_parenth_expr();
    } else {
        auto err_msg = "Found unknown expression token.";
        CompileError(ErrorType::ParseError, err_msg, peek().line, "keyword, integer, parenthesis", peek().val).print();
        exit(EXIT_FAILURE);
    }
}

int Parser::get_binop_precedence(token tok) {
    int precedence = BinaryOpPrecendence[tok];
    if (precedence <= 0) {
        return -1;
    }
    return precedence;
}

std::optional<std::unique_ptr<NodeVariableAssign>> Parser::parse_variable_assign() {
	auto variable = parse_variable();
	auto assignment = peek().val;
	consume();
    auto expr = parse_binary_expr();
    if (peek().type == token::LINEBRK) {
        auto linebreak = consume().val[0];
        return std::make_unique<NodeVariableAssign>(std::move(variable.value()), assignment, std::move(expr.value()));
    } else {
        CompileError(ErrorType::ParseError, "", peek().line, "linebreak", peek().val).print();
        exit(EXIT_FAILURE);
    }
}

std::optional<std::unique_ptr<NodeAST>> Parser::parse_assign_statement() {
	if (peek().type == token::KEYWORD && peek(1).type == token::ASSIGN) {
        return std::make_unique<NodeAssignStatement>(parse_variable_assign().value());
    } else {
        CompileError(ErrorType::ParseError, "", peek().line, "Assignment operator", peek().val).print();
        exit(EXIT_FAILURE);
    }
}

std::optional<std::unique_ptr<NodeStatements>> Parser::parse_statements() {
	while(peek().type == token::LINEBRK){
		consume();
	}
	auto assign_statement = parse_assign_statement();
	std::vector<std::unique_ptr<NodeAST>> stmts;
	if (assign_statement.has_value()) {
		stmts.push_back(std::move(assign_statement.value()));
		return std::make_unique<NodeStatements>(std::move(stmts));
	}
	return {};
}

Result<std::unique_ptr<NodeCallback>> Parser::parse_callback() {
    while(peek().type == token::LINEBRK){
        consume();
    }
	if(peek().type == token::BEGIN_CALLBACK) {
		std::string begin_callback = consume().val;
		if(peek().type == token::LINEBRK) {
			char linebreak = consume().val[0];
			auto stmts = parse_statements();
			if(peek().type == token::END_CALLBACK) {
				std::string end_callback = consume().val;
                auto value = std::make_unique<NodeCallback>(begin_callback,std::move(stmts.value()), end_callback);
				return Result<std::unique_ptr<NodeCallback>>(std::move(value));
			} else {
                return Result<std::unique_ptr<NodeCallback>>(
                        CompileError(ErrorType::ParseError, "", peek().line, "end on", peek().val));
            }
		} else {
            return Result<std::unique_ptr<NodeCallback>>(
                    CompileError(ErrorType::ParseError, "", peek().line, "linebreak", peek().val));
        }
	} else {
        return Result<std::unique_ptr<NodeCallback>>(
                CompileError(ErrorType::ParseError, "", peek().line, "on <callback>", peek().val));
    }
}

