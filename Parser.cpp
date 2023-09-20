//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"
#include "ASTVisitor.h"

#include <filesystem>
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

Result<std::unique_ptr<NodeString>> Parser::parse_string() {
    auto return_value = std::make_unique<NodeString>(std::move(consume().val));
    return Result<std::unique_ptr<NodeString>>(std::move(return_value));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_number() {
    if (peek().type != token::INT && peek().type != token::FLOAT) {
        return Result<std::unique_ptr<NodeAST>>(
                CompileError(ErrorType::ParseError, "Not a number.", peek().line, "INT or REAL", peek().val));
    }
    auto value = consume(); // sollte jetzt entweder INT oder FLOAT sein
    // convert with c methods, error catching
    if (value.type == token::INT) {
        try {
            auto node = std::make_unique<NodeInt>(std::stoi(value.val));
            node->type = ASTType::Integer;
            return Result<std::unique_ptr<NodeAST>>(std::move(node));
        } catch (const std::exception& e) {
            return Result<std::unique_ptr<NodeAST>>(
                    CompileError(ErrorType::ParseError, "Invalid integer format.", peek().line, "valid integer", peek().val));
        }
    } else {
        try {
            auto node = std::make_unique<NodeReal>(std::stof(value.val));
            node->type = ASTType::Real;
            return Result<std::unique_ptr<NodeAST>>(std::move(node));
        } catch (const std::exception& e) {
            return Result<std::unique_ptr<NodeAST>>(
                    CompileError(ErrorType::ParseError, "Invalid real format.", peek().line, "valid real", peek().val));
        }
    }
}

Result<std::unique_ptr<NodeVariable>> Parser::parse_variable() {
    VarType type = VarType::Mutable;
    if(peek().type == token::CONST)
        type = VarType::Const;
    if(peek().type == token::POLYPHONIC)
        type = VarType::Polyphonic;
    // see if variable already has identifier
    char ident = 0;
    std::string var_name = peek().val;
    if (contains(VAR_IDENT, peek().val[0])) {
        ident = peek().val[0];
        var_name = peek().val.substr(1);
    }
    consume();
    auto return_value = std::make_unique<NodeVariable>(var_name, type, ident);
    return Result<std::unique_ptr<NodeVariable>>(std::move(return_value));
}

Result<std::unique_ptr<NodeArray>> Parser::parse_array(std::unique_ptr<NodeVariable> array_variable) {
    std::unique_ptr<NodeParamList> indexes;
	std::unique_ptr<NodeParamList> sizes;
    if(peek().type == token::OPEN_BRACKET) {
        auto index_params = parse_param_list();
        if(index_params.is_error()) {
            return Result<std::unique_ptr<NodeArray>>(index_params.get_error());
        }
		sizes = std::make_unique<NodeParamList>();
        indexes = std::move(index_params.unwrap());
    } else {
        return Result<std::unique_ptr<NodeArray>>(CompileError(ErrorType::SyntaxError,
             "Found unknown Array Syntax.", peek().line, "[", peek().val));
    }
    array_variable->var_type = VarType::Array;
    auto return_value = std::make_unique<NodeArray>(std::move(array_variable), std::move(sizes), std::move(indexes));
    return Result<std::unique_ptr<NodeArray>>(std::move(return_value));
}


Result<std::unique_ptr<NodeAST>> Parser::parse_expression() {
    auto lhs = parse_string_expr();
    if(lhs.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(lhs.get_error());
    }
    return _parse_string_expr_rhs(std::move(lhs.unwrap()));
//    return std::move(lhs);
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_string_expr_rhs(std::unique_ptr<NodeAST> lhs) {
    while(true) {
        Token string_op = peek();
        if(peek().type != token::STRING_OPERATOR) {
            return Result<std::unique_ptr<NodeAST>>(std::move(lhs));
        }
        consume();
        auto rhs = parse_string_expr();
        if(rhs.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
        }
        rhs = _parse_string_expr_rhs(std::move(rhs.unwrap()));
        if (rhs.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
        }
        lhs = std::make_unique<NodeBinaryExpr>(string_op.val, std::move(lhs), std::move(rhs.unwrap()));
        lhs->type = ASTType::String;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_string_expr() {
    if (peek().type == token::STRING) {
        auto expr = parse_string();
        if (!expr.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(std::move(expr.unwrap()));
        } else return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    } else {
        auto expr = parse_binary_expr();
        if (!expr.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(std::move(expr.unwrap()));
        } else return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    }
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
        // is variable or array
        auto var = parse_variable();
        if(var.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(var.get_error());
        }
        if(peek().type == token::OPEN_BRACKET) {
            auto var_array = parse_array(std::move(var.unwrap()));
            if (var_array.is_error()) {
                return Result<std::unique_ptr<NodeAST>>(var_array.get_error());
            }
            return Result<std::unique_ptr<NodeAST>>(std::move(var_array.unwrap()));
        }
        return Result<std::unique_ptr<NodeAST>>(std::move(var.unwrap()));
    // is expression in brackets
    } else if (peek().type == token::OPEN_PARENTH) {
        return _parse_parenth_expr();
    } else if (peek().type == token::INT || peek().type == token::FLOAT) {
        return parse_number();
    // unary operators bool_not, bit_not, sub
    } else if (is_unary_operator(peek().type)){
        return parse_unary_expr();
    } else {
        return Result<std::unique_ptr<NodeAST>>(
                CompileError(ErrorType::ParseError,
                     "Found unknown expression token.", peek().line, "keyword, integer, parenthesis", peek().val));
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_unary_expr() {
    Token unary_op = consume();
    auto expr = parse_binary_expr();
    if(expr.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    }
    auto return_value = std::make_unique<NodeUnaryExpr>(unary_op, std::move(expr.unwrap()));
    return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
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
                return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
                 "Nested Comparisons are not allowed.", peek().line, "valid expression operator", bin_op.val));
            }
            type = ASTType::Comparison;
		} else if (bin_op.type == token::BOOL_AND || bin_op.type == token::BOOL_OR){
			type = ASTType::Boolean;
            if (not(lhs->type == ASTType::Comparison && rhs.unwrap()->type == ASTType::Comparison)) {
                return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
                 "Boolean Operators can only connect Comparisons.", peek().line, "Comparisons"));
            }
		} else if (bin_op.type == token::COMMA) {
			type = ASTType::ParamList;
		}
        // brauch ich das jetzt schon, oder vllt erst nachher beim typisierungs-check?
        if (lhs->type == Integer && rhs.unwrap()->type == Real || lhs->type == Real && rhs.unwrap()->type == Integer) {
            return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
             "Merging of different Expression Types is not allowed.", peek().line, "One Expression Type per Expression", bin_op.val));
        }
        lhs = std::make_unique<NodeBinaryExpr>(bin_op.val, std::move(lhs), std::move(rhs.unwrap()));
        lhs->type = type;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_parenth_expr() {
    consume(); // eat (
    auto expr = parse_binary_expr();
    if (peek().type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::ParseError,
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

Result<std::unique_ptr<NodeAST>> Parser::parse_assign_statement() {
	// make it possible to have more than one variable before assign
	auto var_list = parse_param_list();
	if(var_list.is_error()) {
		return Result<std::unique_ptr<NodeAST>>(var_list.get_error());
	}
	auto vars = std::move(var_list.unwrap());

    if(peek().type == token::ASSIGN) {
        consume(); // consume :=
		auto assignee =  parse_param_list(); //_parse_assignee();
		if(assignee.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(assignee.get_error());
		}
		auto return_value = std::make_unique<NodeAssignStatement>(std::move(vars), std::move(assignee.unwrap()));
		return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
        "Found invalid Assign Statement Syntax.", peek().line, ":=", peek().val));
    }
}

Result<std::unique_ptr<NodeStatement>> Parser::parse_statement() {
    _skip_linebreaks();
    std::unique_ptr<NodeAST> stmt;
    // assign statement
    if (peek().type == token::KEYWORD || peek().type == token::DEFINE || peek().type == token::DECLARE || peek().type == token::CALL) {
        if (peek().type == token::DECLARE) {
			auto declare_stmt = parse_declare_statement();
			if (declare_stmt.is_error()) {
				return Result<std::unique_ptr<NodeStatement>>(declare_stmt.get_error());
			}
			stmt = std::move(declare_stmt.unwrap());
		} else if (peek().type == token::DEFINE) {
			auto define_stmt = parse_define_statement();
			if (define_stmt.is_error()) {
				return Result<std::unique_ptr<NodeStatement>>(define_stmt.get_error());
			}
			stmt = std::move(define_stmt.unwrap());
        } else if ((peek().type == token::CALL) xor (peek(1).type == token::OPEN_PARENTH or peek(1).type == token::LINEBRK)){
            auto function_call = parse_function_call();
            if (function_call.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(function_call.get_error());
            }
            stmt = std::move(function_call.unwrap());
        } else {
            auto assign_stmt = parse_assign_statement();
            if (assign_stmt.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(assign_stmt.get_error());
            }
            stmt = std::move(assign_stmt.unwrap());
        }
    } else if (peek().type == token::IF) {
        auto if_stmt = parse_if_statement();
        if (if_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(if_stmt.get_error());
        }
        stmt = std::move(if_stmt.unwrap());
    } else if (peek().type == token::FOR) {
        auto for_stmt = parse_for_statement();
        if (for_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(for_stmt.get_error());
        }
        stmt = std::move(for_stmt.unwrap());
    } else if (peek().type == token::WHILE) {
        auto while_stmt = parse_while_statement();
        if(while_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(while_stmt.get_error());
        }
        stmt = std::move(while_stmt.unwrap());
	} else if (peek().type == token::SELECT) {
		auto select_stmt = parse_select_statement();
		if(select_stmt.is_error()) {
			return Result<std::unique_ptr<NodeStatement>>(select_stmt.get_error());
		}
		stmt = std::move(select_stmt.unwrap());
    } else {
        return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
         "Found invalid Statement Syntax.", peek().line, "Statement", peek().val));
    }
	if(peek().type == token::LINEBRK) {
		consume();
        _skip_linebreaks();
        auto value = std::make_unique<NodeStatement>(std::move(stmt));
		return Result<std::unique_ptr<NodeStatement>>(std::move(value));
	} else {
		return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
         "Missing linebreak after statement.", peek().line, "linebreak", peek().val));
	}
}

Result<std::unique_ptr<NodeCallback>> Parser::parse_callback() {
    std::string begin_callback = consume().val;
    if(peek().type == token::LINEBRK) {
        consume(); // Consume LINEBREAK
        std::vector<std::unique_ptr<NodeStatement>> stmts;
        while (peek().type != token::END_CALLBACK) {
            if (peek().type == token::BEGIN_CALLBACK) {
                return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
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
        return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
         "", peek().line, "linebreak", peek().val));
    }
}

Result<std::unique_ptr<NodeProgram>> Parser::parse_program() {
    std::vector<std::unique_ptr<NodeImport>> imports;
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
    std::vector<std::unique_ptr<NodeAST>> macro_definitions;
	std::vector<std::unique_ptr<NodeDefineStatement>> defines;
    while (peek().type != token::END_TOKEN) {
        _skip_linebreaks();
        if (peek().type == token::BEGIN_CALLBACK) {
            auto callback = parse_callback();
            if (callback.is_error())
                return Result<std::unique_ptr<NodeProgram>>(callback.get_error());
			callbacks.push_back(std::move(callback.unwrap()));
        } else if (peek().type == token::FUNCTION) {
            auto function = parse_function_definition();
            if (function.is_error())
                return Result<std::unique_ptr<NodeProgram>>(function.get_error());
			function_definitions.push_back(std::move(function.unwrap()));
        } else if (peek().type == token::IMPORT) {
            auto import = parse_import();
            if (import.is_error())
                return Result<std::unique_ptr<NodeProgram>>(import.get_error());
			imports.push_back(std::move(import.unwrap()));
		} else if (peek().type == token::DEFINE) {
			auto define_stmt = parse_define_statement();
			if (define_stmt.is_error())
				return Result<std::unique_ptr<NodeProgram>>(define_stmt.get_error());
			defines.push_back(std::move(define_stmt.unwrap()));
        } else {
            return Result<std::unique_ptr<NodeProgram>>(CompileError(ErrorType::ParseError,
             "", peek().line, "import, define, callback, function, macro", peek().val));
        }
        _skip_linebreaks();
    }
    auto value = std::make_unique<NodeProgram>(std::move(callbacks), std::move(function_definitions), std::move(imports), std::move(macro_definitions), std::move(defines));
    return Result<std::unique_ptr<NodeProgram>>(std::move(value));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_param_list() {
    std::unique_ptr<NodeParamList> params;
    if(peek().type == token::OPEN_BRACKET)
        consume();
    auto param_expression = parse_expression();
	if(param_expression.is_error()) {
		return Result<std::unique_ptr<NodeParamList>>(param_expression.get_error());
	}
	auto param_list = _parse_into_param_list(std::move(param_expression.unwrap()));
	if(param_list.is_error()) {
		return Result<std::unique_ptr<NodeParamList>>(param_list.get_error());
	}
    if(peek().type == token::CLOSED_BRACKET)
        consume();
	auto unwrapped_params = std::move(param_list.unwrap());

	if (dynamic_cast<NodeParamList*>(unwrapped_params.get())) {
		// Wenn variables bereits vom Typ NodeParamList ist, übertrage den Besitz
		params = std::unique_ptr<NodeParamList>(static_cast<NodeParamList*>(unwrapped_params.release()));
	} else {
		// Andernfalls erstelle einen neuen NodeParamList und füge variables hinzu
		params = std::make_unique<NodeParamList>();
		params->params.push_back(std::move(unwrapped_params));
	}
    return Result<std::unique_ptr<NodeParamList>>(std::move(params));
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_into_param_list(std::unique_ptr<NodeAST> expression) {
	auto param_list = std::make_unique<NodeParamList>();

	if (expression->type == ASTType::ParamList) {
		if (auto temp = dynamic_cast<NodeBinaryExpr*>(expression.get())) {
			auto left = std::move(temp->left);
			auto right = std::move(temp->right);

			auto leftResult = _parse_into_param_list(std::move(left));
			param_list->params.push_back(std::move(leftResult.unwrap()));

			auto rightResult = _parse_into_param_list(std::move(right));
			param_list->params.push_back(std::move(rightResult.unwrap()));

			auto result = std::unique_ptr<NodeAST>(std::move(param_list));
			result->type = ASTType::ParamList;
			return Result<std::unique_ptr<NodeAST>>(std::move(result));
		} else {
			return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
			 "Found invalid Parameter List Statement Syntax.", peek().line));
		}
	} else {
		return Result<std::unique_ptr<NodeAST>>(std::move(expression));
	}
}

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header() {
    std::string func_name;
    std::unique_ptr<NodeParamList> func_args = std::make_unique<NodeParamList>();
	func_name = consume().val;
	if (peek().type == token::OPEN_PARENTH) {
		if(peek(1).type != token::CLOSED_PARENTH) {
			auto param_list = parse_param_list();
			if (param_list.is_error()) {
				Result<std::unique_ptr<NodeFunctionHeader>>(param_list.get_error());
			}
			func_args = std::move(param_list.unwrap());
		} else {
			// consume both parentheses and add empty vector as Param List
			consume();
			consume();
		}
	}
    auto value = std::make_unique<NodeFunctionHeader>(func_name, std::move(func_args));
    return Result<std::unique_ptr<NodeFunctionHeader>>(std::move(value));
}

Result<std::unique_ptr<NodeFunctionCall>> Parser::parse_function_call() {
    bool is_call = false;
    if(peek().type == token::CALL) {
        is_call = true;
        consume(); //consume call
    }
    auto func_stmt = parse_function_header();
    if(func_stmt.is_error()){
        return Result<std::unique_ptr<NodeFunctionCall>>(func_stmt.get_error());
    }
    auto return_value = std::make_unique<NodeFunctionCall>(is_call, std::move(func_stmt.unwrap()));
    return Result<std::unique_ptr<NodeFunctionCall>>(std::move(return_value));
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
					return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::ParseError,
                     "Missing return variable after ->",peek().line,"return variable",peek().val));
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
				return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
                 "Missing necessary linebreak after function header.",peek().line,"linebreak",peek().val));
			}
		} else {
			return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
             "Missing function name.",peek().line,"keyword",peek().val));
		}
	} else {
		return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
         "Missing function initializer.",peek().line,"function",peek().val));
	}
    auto value = std::make_unique<NodeFunctionDefinition>(std::move(func_header), std::move(func_return_var), func_override, std::move(func_body));
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(value));
}

Result<std::unique_ptr<NodeImport>> Parser::parse_import() {
    //consume import token IMPORT
    consume();
    if(peek().type ==token::STRING) {
        std::string filepath = consume().val;
//        if (!std::filesystem::exists(filepath)) {
//            return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
//            "Not a valid filepath",peek().line,"valid path",filepath));
//        }
        std::string alias;
        if(peek().type == token::AS) {
            // consume as token
            consume();
            if(peek().type == token::KEYWORD) {
                alias = consume().val;
            } else {
                return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
                "Incorrect import Syntax.",peek().line,"as <keyword>",peek().val));
            }
        }
        auto return_value = std::make_unique<NodeImport>(filepath, alias);
        return Result<std::unique_ptr<NodeImport>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeImport>>(CompileError(ErrorType::ParseError,
        "Not a filepath",peek().line,"path",peek().val));
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_declare_statement() {
	VarType type = VarType::Mutable;
	if(peek().type == token::DECLARE) {
		consume();
		if(peek().type == token::CONST) {
			consume();
			type = VarType::Const;
		} else if (peek().type == token::POLYPHONIC) {
			consume();
			type = VarType::Polyphonic;
		}
	}
    auto variable_declarations = parse_param_list();
	if(variable_declarations.is_error()) {
		return Result<std::unique_ptr<NodeAST>>(variable_declarations.get_error());
	}
	auto variables = std::move(variable_declarations.unwrap());
    for(auto &var: variables->params) {
        if (auto variable = dynamic_cast<NodeVariable*>(var.get())) {
            // var ist eine Instanz von NodeVariable
            variable->var_type = type;
        } else if (auto array = dynamic_cast<NodeArray*>(var.get())) {
			// var ist eine Instanz von NodeArray
			std::swap(array->indexes, array->sizes);
		} else if (auto function = dynamic_cast<NodeFunctionHeader*>(var.get())) {

        } else {
            // var ist weder NodeVariable noch NodeArray
            return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
            "Did not find variable.",peek().line, "variable/array", peek().val));
        }
    }

	std::unique_ptr<NodeParamList> assignees = nullptr;
	// if there is an assignment following
	if (peek().type == token::ASSIGN) {
        consume(); //consume :=
		auto assignee = parse_param_list();
		if(assignee.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(assignee.get_error());
		}
		assignees = std::move(assignee.unwrap());
	} else
	    // initializes empty param list
	    assignees = std::make_unique<NodeParamList>();

	auto return_value = std::make_unique<NodeDeclareStatement>(std::move(variables), std::move(assignees));
	return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
}

Result<std::unique_ptr<NodeIfStatement>> Parser::parse_if_statement() {
    //consume if
    consume();
    auto condition_result = parse_expression();
    if(condition_result.is_error()) {
        return Result<std::unique_ptr<NodeIfStatement>>(condition_result.get_error());
    }
    auto condition = std::move(condition_result.unwrap());
    if(not(condition->type == ASTType::Boolean || condition->type == ASTType::Comparison)) {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
        "If Statement needs condition.", peek().line, "condition"));
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
         "Expected linebreak after if-condition.", peek().line, "linebreak", peek().val));
    }
    std::vector<std::unique_ptr<NodeStatement>> if_stmts = {};
    while (peek().type != token::END_IF && peek().type != token::ELSE) {
        auto stmt = parse_statement();
        if (!stmt.is_error()) {
            if_stmts.push_back(std::move(stmt.unwrap()));
        } else {
            return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
        }
    }
    bool no_end_if = false;
    std::vector<std::unique_ptr<NodeStatement>> else_stmts = {};
    if(peek().type == token::ELSE) {
        consume();
        if(not(peek().type == token::IF || peek().type == token::LINEBRK)) {
            return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
            "Expected linebreak after else-condition.", peek().line, "linebreak", peek().val));
        }
        if(peek().type == token::IF) {
            no_end_if = true;
            auto stmt = parse_if_statement();
            if (stmt.is_error()) {
                return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
            }
            auto stmt_val = std::make_unique<NodeStatement>(std::move(stmt.unwrap()));
            else_stmts.push_back(std::move(stmt_val));
        } else {
            while (peek().type != token::END_IF) {
                auto stmt = parse_statement();
                if (stmt.is_error()) {
                    return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
                }
                else_stmts.push_back(std::move(stmt.unwrap()));
            }
        }
    }
    if(peek().type == token::END_IF || no_end_if) {
        if (!no_end_if) consume();
        auto return_value = std::make_unique<NodeIfStatement>(std::move(condition), std::move(if_stmts), std::move(else_stmts));
        return Result<std::unique_ptr<NodeIfStatement>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
         "Missing end token.", peek().line, "end if", peek().val));
    }

}

Result<std::unique_ptr<NodeForStatement>> Parser::parse_for_statement() {
    //consume for
    consume();
    auto assign_stmt = parse_assign_statement();
    if(assign_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForStatement>>(assign_stmt.get_error());
    }
    auto iterator = std::move(assign_stmt.unwrap());
    if(not(peek().type == token::TO || peek().type == token::DOWNTO)) {
        return Result<std::unique_ptr<NodeForStatement>>(CompileError(ErrorType::SyntaxError,
        "Incorrect for loop Syntax.", peek().line, "to/downto", peek().val));
    }
    Token to = consume(); //consume to or downto
    auto expression_stmt = parse_binary_expr();
    if(expression_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForStatement>>(expression_stmt.get_error());
    }
    auto iterator_end = std::move(expression_stmt.unwrap());
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeForStatement>>(CompileError(ErrorType::SyntaxError,
         "Missing linebreak in for loop", peek().line, "linebreak", peek().val));
    }
    consume(); //consume linebreak
    std::vector<std::unique_ptr<NodeStatement>> stmts = {};
    while (peek().type != token::END_FOR) {
        auto stmt = parse_statement();
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeForStatement>>(stmt.get_error());
        }
        stmts.push_back(std::move(stmt.unwrap()));
    }
    consume(); // consume end for

	std::vector<std::unique_ptr<NodeAST>> iterator_param_list;
	iterator_param_list.push_back(std::move(iterator));
	auto args = std::make_unique<NodeParamList>(std::move(iterator_param_list));
	auto inc_func_header = std::make_unique<NodeFunctionHeader>("inc", std::move(args));
	auto inc_func_call = std::make_unique<NodeFunctionCall>(false, std::move(inc_func_header));
	auto inc_func_stmt = std::make_unique<NodeStatement>(std::move(inc_func_call));
	stmts.push_back(std::move(inc_func_stmt));
    auto return_value = std::make_unique<NodeForStatement>(std::move(iterator), to, std::move(iterator_end), std::move(stmts));
    return Result<std::unique_ptr<NodeForStatement>>(std::move(return_value));
}

Result<std::unique_ptr<NodeWhileStatement>> Parser::parse_while_statement() {
    consume(); // consume while
    auto condition_result = parse_expression();
    if(condition_result.is_error()) {
        return Result<std::unique_ptr<NodeWhileStatement>>(condition_result.get_error());
    }
    auto condition = std::move(condition_result.unwrap());
    if(not(condition->type == ASTType::Boolean || condition->type == ASTType::Comparison)) {
        return Result<std::unique_ptr<NodeWhileStatement>>(CompileError(ErrorType::SyntaxError,
         "While Statement needs condition.", peek().line, "condition"));
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeWhileStatement>>(CompileError(ErrorType::SyntaxError,
         "Expected linebreak after while-condition.", peek().line, "linebreak", peek().val));
    }
    consume(); //consume linebreak
    std::vector<std::unique_ptr<NodeStatement>> stmts = {};
    while (peek().type != token::END_WHILE) {
        auto stmt = parse_statement();
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeWhileStatement>>(stmt.get_error());
        }
        stmts.push_back(std::move(stmt.unwrap()));
    }
    consume(); // consume end for
    auto return_value = std::make_unique<NodeWhileStatement>(std::move(condition), std::move(stmts));
    return Result<std::unique_ptr<NodeWhileStatement>>(std::move(return_value));
}

Result<std::unique_ptr<NodeSelectStatement>> Parser::parse_select_statement() {
	consume(); //consume select
	auto expression = parse_expression();
	if(peek().type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		"Expected linebreak after select-expression.", peek().line, "linebreak", peek().val));
	}
	consume(); //consume linebreak
	if(peek().type != token::CASE) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		 "Expected cases in select-expression.", peek().line, "case <expression>", peek().val));
	}
	std::map<std::unique_ptr<NodeAST>, std::vector<std::unique_ptr<NodeStatement>>> cases;
	while (peek().type != token::END_SELECT) {
		if(peek().type == token::CASE) {
			consume(); //consume case
			auto cas = parse_expression();
			if(cas.is_error()) {
				return Result<std::unique_ptr<NodeSelectStatement>>(cas.get_error());
			}
			if(peek().type != token::LINEBRK) {
				return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
				 "Expected linebreak after case.", peek().line, "linebreak", peek().val));
			}
			consume(); //consume linebreak
			std::vector<std::unique_ptr<NodeStatement>> stmts = {};
			while(peek().type != token::END_SELECT && peek().type != token::CASE) {
				auto stmt = parse_statement();
				if (stmt.is_error()) {
					return Result<std::unique_ptr<NodeSelectStatement>>(stmt.get_error());
				}
				stmts.push_back(std::move(stmt.unwrap()));
			}
			cases.insert(std::make_pair(std::move(cas.unwrap()),std::move( stmts)));
		}
	}
	consume(); // consume end select
	auto return_value = std::make_unique<NodeSelectStatement>(std::move(expression.unwrap()), std::move(cases));
	return Result<std::unique_ptr<NodeSelectStatement>>(std::move(return_value));
}

Result<std::unique_ptr<NodeDefineStatement>> Parser::parse_define_statement() {
	consume(); //consume define keyword
	VarType type = VarType::Define;
	auto definitions = parse_param_list();
	if(definitions.is_error()) {
		return Result<std::unique_ptr<NodeDefineStatement>>(definitions.get_error());
	}
	if(peek().type != token::ASSIGN) {
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
		 "Defines need to have expressions assigned.", peek().line, ":=", peek().val));
	}
	consume(); // consume assign :=
	auto assignees = parse_param_list();
	if(assignees.is_error()) {
		return Result<std::unique_ptr<NodeDefineStatement>>(assignees.get_error());
	}
	auto return_value = std::make_unique<NodeDefineStatement>(std::move(definitions.unwrap()), std::move(assignees.unwrap()));
	return Result<std::unique_ptr<NodeDefineStatement>>(std::move(return_value));
}






