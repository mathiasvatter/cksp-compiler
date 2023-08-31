//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"
#include "ASTVisitor.h"

#include <utility>

Parser::Parser(std::vector<Token> tokens): m_tokens(std::move(tokens)) {
	m_pos = 0;
    curr_token = m_tokens.at(0).type;

	ASTPrinter printer;
    auto prog = parse_program();
    if (prog.is_error())
        prog.get_error().print();
    else
	    prog.unwrap()->accept(printer);
//    parse_variable_assign().value();

}

Token Parser::peek(int ahead) {
	if (m_tokens.size() < m_pos+ahead) {
        auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
        CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val).print();
        exit(EXIT_FAILURE);
    }
	curr_token = m_tokens.at(m_pos+ahead).type;
	return m_tokens.at(m_pos+ahead);

}

Token Parser::consume() {
    if (m_pos < m_tokens.size()) {
        curr_token = m_tokens.at(m_pos + 1).type;
        return m_tokens.at(m_pos++);
    }
    auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
    CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val).print();
    exit(EXIT_FAILURE);
}

void Parser::_skip_linebreaks() {
    while(peek().type == token::LINEBRK){
        consume();
    }
}

std::optional<std::unique_ptr<NodeInt>> Parser::parse_int() {
    std::string value;
    if (peek().type == token::INT) {
        value = consume().val;
    } else if (peek().type == token::SUB && peek(1).type == token::INT) {
        value = consume().val;
        value += consume().val;
    } else {
        auto err_msg = "Found unknown Integer Syntax.";
        CompileError(ErrorType::ParseError, err_msg, peek().line, "(negative) Integer", peek().val).print();
        exit(EXIT_FAILURE);
    }
    try {
        return std::make_unique<NodeInt>(std::stoi(value));
    } catch (const std::exception& e) {
        auto err_msg = "Did not recognize Integer.";
        CompileError(ErrorType::ParseError, err_msg, peek().line, "Integer", peek().val).print();
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
        int prec = _get_binop_precedence(peek().type);
        if(prec < precedence)
            return lhs;
        // its not -1 so it is a binop
        auto bin_op = peek().val;
        consume();
        auto rhs = _parse_primary_expr();
        if (!rhs.has_value()) {
            return {};
        }
        int next_precedence = _get_binop_precedence(peek().type);
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
    } else if (peek().type == token::INT || (peek().type == token::SUB && peek(1).type == token::INT)) {
        return parse_int();
    } else if (peek().type == token::OPEN_PARENTH) {
        return _parse_parenth_expr();
    } else {
        auto err_msg = "Found unknown expression token.";
        CompileError(ErrorType::ParseError, err_msg, peek().line, "keyword, integer, parenthesis", peek().val).print();
        exit(EXIT_FAILURE);
    }
}

int Parser::_get_binop_precedence(token tok) {
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
	return std::make_unique<NodeVariableAssign>(std::move(variable.value()), assignment, std::move(expr.value()));
}

std::optional<std::unique_ptr<NodeAssignStatement>> Parser::parse_assign_statement() {
	if (peek().type == token::KEYWORD && peek(1).type == token::ASSIGN) {
        return std::make_unique<NodeAssignStatement>(parse_variable_assign().value());
    } else {
        CompileError(ErrorType::ParseError, "", peek().line, "Assignment operator", peek().val).print();
        exit(EXIT_FAILURE);
    }
}

std::optional<std::unique_ptr<NodeStatement>> Parser::parse_statement() {
	_skip_linebreaks();
	auto stmt = parse_assign_statement();
	if (!stmt.has_value()) {
		return {};
	}
	if(peek().type == token::LINEBRK) {
		consume();
		return std::make_unique<NodeStatement>(std::move(stmt.value()));
	} else {
		CompileError(ErrorType::SyntaxError, "Missing linebreak after statement.", peek().line, "linebreak", peek().val).print();
		exit(EXIT_FAILURE);
	}
}

Result<std::unique_ptr<NodeCallback>> Parser::parse_callback() {
	_skip_linebreaks();
    std::string begin_callback = consume().val;
    if(peek().type == token::LINEBRK) {
        char linebreak = consume().val[0];
        std::vector<std::unique_ptr<NodeStatement>> stmts;
        while (peek().type != token::END_CALLBACK) {
            if (peek().type == token::BEGIN_CALLBACK) {
                return Result<std::unique_ptr<NodeCallback>>(
                    CompileError(ErrorType::ParseError, "", peek().line, "end on", peek().val));
            }
            auto stmt = parse_statement();
            if (stmt.has_value()) {
                stmts.push_back(std::move(stmt.value()));
            } else {
                return Result<std::unique_ptr<NodeCallback>>(
                    CompileError(ErrorType::ParseError, "Found unknown statement", peek().line, "valid statement",peek().val));
            }
        }
        std::string end_callback = consume().val;
        auto value = std::make_unique<NodeCallback>(begin_callback,std::move(stmts), end_callback);
        return Result<std::unique_ptr<NodeCallback>>(std::move(value));
    } else {
        return Result<std::unique_ptr<NodeCallback>>(
                CompileError(ErrorType::ParseError, "", peek().line, "linebreak", peek().val));
    }
}

Result<std::unique_ptr<NodeProgram>> Parser::parse_program() {
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
    std::vector<std::unique_ptr<NodeAST>> macro_definitions;
    while (peek().type != token::END_TOKEN) {
        _skip_linebreaks();
        if (peek().type == token::BEGIN_CALLBACK) {
            auto callback = parse_callback();
            if (!callback.is_error()) {
                callbacks.push_back(std::move(callback.unwrap()));
            } else
                return Result<std::unique_ptr<NodeProgram>>(callback.get_error());
        } else {
            return Result<std::unique_ptr<NodeProgram>>(
                    CompileError(ErrorType::ParseError, "", peek().line, "on <callback>", peek().val));
        }
        _skip_linebreaks();

    }
    auto value = std::make_unique<NodeProgram>(std::move(callbacks), std::move(function_definitions), std::move(macro_definitions));
    return Result<std::unique_ptr<NodeProgram>>(std::move(value));
}

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header() {
    std::string func_name;
    std::vector<std::unique_ptr<NodeAST>> func_args;
    if(peek().type == token::FUNCTION) {
        consume();
        if(peek().type == token::KEYWORD) {
            func_name = consume().val;
            if (peek().type == token::OPEN_PARENTH) {
                while(peek().type != token::CLOSED_PARENTH) {
                    auto func_arg = parse_binary_expr();
                    if (func_arg.has_value()) {
                        func_args.push_back(std::move(func_arg.value()));
                    }
                    if (peek().type == token::COMMA) {
                        consume();
                    } else {
                        return Result<std::unique_ptr<NodeFunctionHeader>>(
                                CompileError(ErrorType::SyntaxError, "Function args not comma separated.", peek().line, ",", peek().val));
                    }
                }
                consume();
            } else {
                return Result<std::unique_ptr<NodeFunctionHeader>>(
                        CompileError(ErrorType::SyntaxError, "Function keywords need to be followed by parenthesis.", peek().line, ")", peek().val));
            }
        }  else {
            return Result<std::unique_ptr<NodeFunctionHeader>>(
                    CompileError(ErrorType::SyntaxError, "", peek().line, "function name", peek().val));
        }
    }
    auto value = std::make_unique<NodeFunctionHeader>(func_name, std::move(func_args));
    return Result<std::unique_ptr<NodeFunctionHeader>>(std::move(value));
}

Result<std::unique_ptr<NodeFunctionDefinition>> Parser::parse_function_definition() {
    std::unique_ptr<NodeFunctionHeader> func_header;
    std::unique_ptr<NodeVariable> func_return_var;
    std::vector<std::unique_ptr<NodeStatement>> func_body;
    bool func_override = false;
    if(peek().type == token::FUNCTION) {
        auto header = parse_function_header();
        if(!header.is_error()) {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(header.get_error());
        }
        func_header = std::move(header.unwrap());
        if(peek().type == token::ASSIGN) {
            consume();
            if(peek().type == token::KEYWORD) {
                auto return_var = parse_variable();
                if(return_var.has_value()) {
                    func_return_var = std::move(return_var.value());
                }
            }
        }
        if(peek().type == token::OVERRIDE) {
            consume();
            func_override = true;
        }
        if(peek().type == token::LINEBRK) {
            while (peek().type != token::END_FUNCTION) {
                auto stmt = parse_statement();
                if (stmt.has_value()) {
                    func_body.push_back(std::move(stmt.value()));
                } else {
                    return Result<std::unique_ptr<NodeFunctionDefinition>>(
                            CompileError(ErrorType::ParseError, "Found unknown statement", peek().line, "valid statement",peek().val));
                }
            }
            consume();
        }
    }
    auto value = std::make_unique<NodeFunctionDefinition>(std::move(func_header), std::move(func_return_var), func_override, std::move(func_body));
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(value));
}



