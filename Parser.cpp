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

Result<std::unique_ptr<NodeAST>> Parser::parse_number() {
    std::string value;
    bool is_negative = false;
    if (peek().type == token::SUB && (peek(1).type == token::INT || peek(1).type == token::FLOAT)) {
        consume(); // Konsumiere den '-'
        is_negative = true;
    }
    if (peek().type != token::INT && peek().type != token::FLOAT) {
        return Result<std::unique_ptr<NodeAST>>(
                CompileError(ErrorType::ParseError, "Expected a number.", peek().line, "INT or REAL", peek().val));
    }
    Token next = consume(); // sollte jetzt entweder INT oder FLOAT sein
    value = next.val;
    if (is_negative) value = "-" + value;

    if (next.type == token::INT) {
        try {
            auto node = std::make_unique<NodeInt>(std::stoi(value));
            node->type = ASTType::Integer;
            return Result<std::unique_ptr<NodeAST>>(std::move(node));
        } catch (const std::exception& e) {
            return Result<std::unique_ptr<NodeAST>>(
                    CompileError(ErrorType::ParseError, "Invalid integer format.", peek().line, "valid integer", peek().val));
        }
    } else {
        try {
            auto node = std::make_unique<NodeReal>(std::stof(value));
            node->type = ASTType::Real;
            return Result<std::unique_ptr<NodeAST>>(std::move(node));
        } catch (const std::exception& e) {
            return Result<std::unique_ptr<NodeAST>>(
                    CompileError(ErrorType::ParseError, "Invalid real format.", peek().line, "valid real", peek().val));
        }
    }
}

Result<std::unique_ptr<NodeVariable>> Parser::parse_variable() {
    // see if variable already has identifier
    char ident = 0;
    std::string var_name = peek().val;
    if (contains(VAR_IDENT, peek().val[0])) {
        ident = peek().val[0];
        var_name = peek().val.substr(1);
    }
    consume();
    auto return_value = std::make_unique<NodeVariable>(var_name, ident);
    return Result<std::unique_ptr<NodeVariable>>(std::move(return_value));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_binary_expr() {
    auto lhs = _parse_primary_expr();
    if(!lhs.is_error()) {
        return _parse_binary_expr_rhs(0, std::move(lhs.unwrap()));
    }
    return Result<std::unique_ptr<NodeAST>>(lhs.get_error());
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_primary_expr() {
    if (peek().type == token::KEYWORD) {
        // is function
		if (peek(1).type == token::OPEN_PARENTH) {
			auto var_function = parse_function_header();
			if (!var_function.is_error()) {
				return Result<std::unique_ptr<NodeAST>>(std::move(var_function.unwrap()));
			}
			return Result<std::unique_ptr<NodeAST>>(var_function.get_error());
		}
        // is variable
        auto var = parse_variable();
        if(!var.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(std::move(var.unwrap()));
        }
        return Result<std::unique_ptr<NodeAST>>(var.get_error());
    // is expression in brackets
    } else if (peek().type == token::OPEN_PARENTH) {
		return _parse_parenth_expr();
    } else {
        auto integer = parse_number();
        if (!integer.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(std::move(integer.unwrap()));
        }
        return Result<std::unique_ptr<NodeAST>>(integer.get_error());
    }
//    } else {
//        return Result<std::unique_ptr<NodeAST>>(
//                CompileError(ErrorType::ParseError,
//							 "Found unknown expression token.",
//							 peek().line, "keyword, integer, parenthesis", peek().val));
//    }
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs) {
    while(true) {
        int prec = _get_binop_precedence(peek().type);
        if(prec < precedence)
            return Result<std::unique_ptr<NodeAST>>(std::move(lhs));
        // its not -1 so it is a binop
        auto bin_op = peek();
        consume();
        auto rhs = _parse_primary_expr();
        if (rhs.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
        }
        int next_precedence = _get_binop_precedence(peek().type);
        if (prec < next_precedence) {
            rhs = _parse_binary_expr_rhs(prec + 1, std::move(rhs.unwrap()));
            if (rhs.is_error()) {
                return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
            }
        }
        ASTType type = ASTType::Unknown;
		if (bin_op.type == token::COMPARISON) {
            //Check if rhs is NodeComparisonExpr because comparisons in comparisons are not allowed
            if (lhs->type == ASTType::Comparison) {
                return Result<std::unique_ptr<NodeAST>>(
                        CompileError(ErrorType::SyntaxError,
                                     "Nested Comparisons are not allowed.",
                                     peek().line, "valid expression operator", bin_op.val));
            }
            type = ASTType::Comparison;
		} else if (bin_op.type == token::BOOL){
			type = ASTType::Boolean;
		}
        // brauch ich das jetzt schon, oder vllt erst nachher beim typisierungs-check?
        if (lhs->type == Integer && rhs.unwrap()->type == Real || lhs->type == Real && rhs.unwrap()->type == Integer) {
            return Result<std::unique_ptr<NodeAST>>(
                    CompileError(ErrorType::SyntaxError,
                                 "Merging of different Expression Types is not allowed.",
                                 peek().line, "One Expression Type per Expression", bin_op.val));
        }
        lhs = std::make_unique<NodeBinaryExpr>(bin_op.val, std::move(lhs), std::move(rhs.unwrap()));
        lhs->type = type;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_parenth_expr() {
    consume(); // eat (
    auto expr = parse_binary_expr();
    if (peek().type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<NodeAST>>(
        	CompileError(ErrorType::ParseError,
						 "Missing parenthesis.", peek().line, ")", peek().val));
    }
    consume(); // eat )
    return expr;
}

int Parser::_get_binop_precedence(token tok) {
    int precedence = BinaryOpPrecendence[tok];
    if (precedence <= 0) {
        return -1;
    }
    return precedence;
}

Result<std::unique_ptr<NodeVariableAssign>> Parser::parse_variable_assign() {
	auto variable = parse_variable();
    if(variable.is_error()) {
        return Result<std::unique_ptr<NodeVariableAssign>>(variable.get_error());
    }
	auto assignment = peek().val;
	consume();
    auto expr = parse_binary_expr();
    if(expr.is_error()) {
        return Result<std::unique_ptr<NodeVariableAssign>>(expr.get_error());
    }
    auto value = std::make_unique<NodeVariableAssign>(std::move(variable.unwrap()), assignment, std::move(expr.unwrap()));
	return Result<std::unique_ptr<NodeVariableAssign>>(std::move(value));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_assign_statement() {
    auto variable_assign = parse_variable_assign();
    if(variable_assign.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(variable_assign.get_error());
    }
    auto value = std::make_unique<NodeAssignStatement>(std::move(variable_assign.unwrap()));
    return Result<std::unique_ptr<NodeAST>>(std::move(value));
}

Result<std::unique_ptr<NodeStatement>> Parser::parse_statement() {
	_skip_linebreaks();
    std::unique_ptr<NodeAST> stmt;
    // assign statement
    if (peek().type == token::KEYWORD && peek(1).type == token::ASSIGN) {
        auto assign_stmt = parse_assign_statement();
        if (assign_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(assign_stmt.get_error());
        }
        stmt = std::move(assign_stmt.unwrap());
    } else {
        return Result<std::unique_ptr<NodeStatement>>(
                CompileError(ErrorType::SyntaxError,
							 "Found invalid Statement Syntax.", peek().line, "Assign Statement", peek().val));
    }
	if(peek().type == token::LINEBRK) {
		consume();
        auto value = std::make_unique<NodeStatement>(std::move(stmt));
		return Result<std::unique_ptr<NodeStatement>>(std::move(value));
	} else {
		return Result<std::unique_ptr<NodeStatement>>(
                CompileError(ErrorType::SyntaxError,
							 "Missing linebreak after statement.", peek().line, "linebreak", peek().val));
	}
}

Result<std::unique_ptr<NodeCallback>> Parser::parse_callback() {
	_skip_linebreaks();
    std::string begin_callback = consume().val;
    if(peek().type == token::LINEBRK) {
        consume(); // Consume LINEBREAK
        std::vector<std::unique_ptr<NodeStatement>> stmts;
        while (peek().type != token::END_CALLBACK) {
            if (peek().type == token::BEGIN_CALLBACK) {
                return Result<std::unique_ptr<NodeCallback>>(
                    CompileError(ErrorType::ParseError,
								 "", peek().line, "end on", peek().val));
            }
            auto stmt = parse_statement();
            if (!stmt.is_error()) {
                stmts.push_back(std::move(stmt.unwrap()));
            } else {
                return Result<std::unique_ptr<NodeCallback>>(stmt.get_error());
            }
        }
        std::string end_callback = consume().val; // Consume END_CALLBACK
        auto value = std::make_unique<NodeCallback>(begin_callback,std::move(stmts), end_callback);
        return Result<std::unique_ptr<NodeCallback>>(std::move(value));
    } else {
        return Result<std::unique_ptr<NodeCallback>>(
                CompileError(ErrorType::ParseError,
							 "", peek().line, "linebreak", peek().val));
    }
}

Result<std::unique_ptr<NodeProgram>> Parser::parse_program() {
    std::vector<std::unique_ptr<NodeImport>> imports;
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
        } else if (peek().type == token::FUNCTION) {
            auto function = parse_function_definition();
            if(!function.is_error()) {
                function_definitions.push_back(std::move(function.unwrap()));
            } else
                return Result<std::unique_ptr<NodeProgram>>(function.get_error());
        } else {
            return Result<std::unique_ptr<NodeProgram>>(
                    CompileError(ErrorType::ParseError,
								 "", peek().line, "callback, function, macro", peek().val));
        }
        _skip_linebreaks();

    }
    auto value = std::make_unique<NodeProgram>(std::move(callbacks), std::move(function_definitions), std::move(imports), std::move(macro_definitions));
    return Result<std::unique_ptr<NodeProgram>>(std::move(value));
}

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header() {
    std::string func_name;
    std::vector<std::unique_ptr<NodeAST>> func_args;
	func_name = consume().val;
	if (peek().type == token::OPEN_PARENTH) {
		consume();
		while(peek().type != token::CLOSED_PARENTH) {
			auto func_arg = parse_binary_expr();
			if (!func_arg.is_error()) {
				func_args.push_back(std::move(func_arg.unwrap()));
			} else
				return Result<std::unique_ptr<NodeFunctionHeader>>(func_arg.get_error());
			if (peek().type == token::COMMA) {
				consume();
			}
		}
		consume();
	} else {
		return Result<std::unique_ptr<NodeFunctionHeader>>(
				CompileError(ErrorType::SyntaxError,
							 "Function keywords need to be followed by parenthesis.",
							 peek().line, ")", peek().val));
	}
    auto value = std::make_unique<NodeFunctionHeader>(func_name, std::move(func_args));
    return Result<std::unique_ptr<NodeFunctionHeader>>(std::move(value));
}

Result<std::unique_ptr<NodeFunctionDefinition>> Parser::parse_function_definition() {
    std::unique_ptr<NodeFunctionHeader> func_header;
    std::optional<std::unique_ptr<NodeVariable>> func_return_var;
    std::vector<std::unique_ptr<NodeStatement>> func_body;
    bool func_override = false;
    if(peek().type == token::FUNCTION) {
		consume();
		if (peek().type == token::KEYWORD) {
			auto header = parse_function_header();
			if (!header.is_error()) {
				func_header = std::move(header.unwrap());
			} else
				return Result<std::unique_ptr<NodeFunctionDefinition>>(header.get_error());
			if (peek().type == token::ARROW) {
				consume();
				if (peek().type == token::KEYWORD) {
					auto return_var = parse_variable();
					if (!return_var.is_error()) {
						func_return_var = std::move(return_var.unwrap());
					}
				} else {
					return Result<std::unique_ptr<NodeFunctionDefinition>>(
						CompileError(ErrorType::ParseError,
									 "Missing return variable after ->",
									 peek().line,"return variable",peek().val));
				}
			} else func_return_var = {};
			if (peek().type == token::OVERRIDE) {
				consume();
				func_override = true;
			}
			if (peek().type == token::LINEBRK) {
				while (peek().type != token::END_FUNCTION) {
					auto stmt = parse_statement();
					if (!stmt.is_error()) {
						func_body.push_back(std::move(stmt.unwrap()));
					} else {
						return Result<std::unique_ptr<NodeFunctionDefinition>>(stmt.get_error());
					}
				}
				consume();
			} else {
				return Result<std::unique_ptr<NodeFunctionDefinition>>(
					CompileError(ErrorType::SyntaxError,
								 "Missing necessary linebreak after function header.",
								 peek().line,"linebreak",peek().val));
			}
		} else {
			return Result<std::unique_ptr<NodeFunctionDefinition>>(
				CompileError(ErrorType::SyntaxError,
							 "Missing function name.",
							 peek().line,"keyword",peek().val));
		}
	} else {
		return Result<std::unique_ptr<NodeFunctionDefinition>>(
			CompileError(ErrorType::SyntaxError,
						 "Missing function initializer.",
						 peek().line,"function",peek().val));
	}
    auto value = std::make_unique<NodeFunctionDefinition>(std::move(func_header), std::move(func_return_var), func_override, std::move(func_body));
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(value));
}



