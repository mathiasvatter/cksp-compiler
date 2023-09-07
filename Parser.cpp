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

Result<std::unique_ptr<NodeArray>> Parser::parse_array(std::unique_ptr<NodeVariable> array_variable, bool is_size) {
//    std::unique_ptr<NodeAST> size;
    std::unique_ptr<NodeParamList> indexes;
	std::unique_ptr<NodeParamList> sizes;
    if(peek().type == token::OPEN_BRACKET) {
        auto index_params = parse_param_list(token::CLOSED_BRACKET);
        if(index_params.is_error()) {
            return Result<std::unique_ptr<NodeArray>>(index_params.get_error());
        }
		sizes = std::make_unique<NodeParamList>();
//		if (is_size) {
//			sizes = std::move(index_params.unwrap());
//		} else {
			indexes = std::move(index_params.unwrap());
//		}
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

Result<std::unique_ptr<NodeAST>> Parser::parse_assign_statement() {
	// make it possible to have more than one variable before assign
	auto var_list = parse_param_list(token::ASSIGN);
	if(var_list.is_error()) {
		return Result<std::unique_ptr<NodeAST>>(var_list.get_error());
	}
	auto vars = std::move(var_list.unwrap());
//    auto value = std::unique_ptr<NodeAST>();
//    auto var = parse_variable();
//    if(var.is_error()) {
//        return Result<std::unique_ptr<NodeAST>>(var.get_error());
//    }
//    // array assignment
//    if(peek().type == token::OPEN_BRACKET) {
//        auto array = parse_array(std::move(var.unwrap()));
//        if(array.is_error()) {
//            return Result<std::unique_ptr<NodeAST>>(array.get_error());
//        }
//        value = std::move(array.unwrap());
//    } else
//        value = std::move(var.unwrap());
    if(peek().type == token::ASSIGN) {
		auto assignee = _parse_assignee();
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

Result<std::unique_ptr<NodeParamList>> Parser::_parse_assignee() {
	consume();
	// array initializer
	token end_token = token::LINEBRK;
	if(peek().type == token::OPEN_PARENTH) {
		end_token = token::CLOSED_PARENTH;
	}
	auto assignee = parse_param_list(end_token);
	if(assignee.is_error()) {
		return Result<std::unique_ptr<NodeParamList>>(assignee.get_error());
	}
	return Result<std::unique_ptr<NodeParamList>>(std::move(assignee.unwrap()));
}

Result<std::unique_ptr<NodeStatement>> Parser::parse_statement() {
    _skip_linebreaks();
    std::unique_ptr<NodeAST> stmt;
    // assign statement
    if (peek().type == token::KEYWORD || peek().type == token::DEFINE || peek().type == token::DECLARE) {
		if(peek().type == token::DEFINE || peek().type == token::DECLARE) {
			auto declare_stmt = parse_declare_statement();
			if(declare_stmt.is_error()) {
				return Result<std::unique_ptr<NodeStatement>>(declare_stmt.get_error());
			}
			stmt = std::move(declare_stmt.unwrap());
		} else {
			auto assign_stmt = parse_assign_statement();
			if (assign_stmt.is_error()) {
				return Result<std::unique_ptr<NodeStatement>>(assign_stmt.get_error());
			}
			stmt = std::move(assign_stmt.unwrap());
		}
    } else {
        return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
         "Found invalid Statement Syntax.", peek().line, "Assign Statement", peek().val));
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
            if (!function.is_error()) {
                function_definitions.push_back(std::move(function.unwrap()));
            } else
                return Result<std::unique_ptr<NodeProgram>>(function.get_error());
        } else if (peek().type == token::IMPORT) {
            auto import = parse_import();
            if (!import.is_error()) {
                imports.push_back(std::move(import.unwrap()));
            } else
                return Result<std::unique_ptr<NodeProgram>>(import.get_error());
        } else {
            return Result<std::unique_ptr<NodeProgram>>(CompileError(ErrorType::ParseError,
             "", peek().line, "callback, function, macro", peek().val));
        }
        _skip_linebreaks();
    }
    auto value = std::make_unique<NodeProgram>(std::move(callbacks), std::move(function_definitions), std::move(imports), std::move(macro_definitions));
    return Result<std::unique_ptr<NodeProgram>>(std::move(value));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_param_list(token end) {
    std::vector<std::unique_ptr<NodeAST>> params;
    if(end == token::CLOSED_PARENTH || end == token::CLOSED_BRACKET)
        consume();
    while(peek().type != end && peek().type != token::LINEBRK) {
        auto param = parse_expression();
        if (!param.is_error()) {
            params.push_back(std::move(param.unwrap()));
        } else
            return Result<std::unique_ptr<NodeParamList>>(param.get_error());
        if (peek().type == token::COMMA) {
            consume();
        }
    }
    // if current token is not ) or ] do not consume, the next node needs it
    if(end == token::CLOSED_PARENTH || end == token::CLOSED_BRACKET)
        consume();
    auto return_value = std::make_unique<NodeParamList>(std::move(params));
    return Result<std::unique_ptr<NodeParamList>>(std::move(return_value));
}

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header() {
    std::string func_name;
    std::unique_ptr<NodeParamList> func_args;
	func_name = consume().val;
	if (peek().type == token::OPEN_PARENTH) {
        auto func_arguments = parse_param_list(token::CLOSED_PARENTH);
        if (func_arguments.is_error()) {
            return Result<std::unique_ptr<NodeFunctionHeader>>(func_arguments.get_error());
        }
        func_args = std::move(func_arguments.unwrap());
	} else {
		return Result<std::unique_ptr<NodeFunctionHeader>>(CompileError(ErrorType::SyntaxError,
         "Function keywords need to be followed by parenthesis.",peek().line, ")", peek().val));
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
	if(peek().type ==token::DEFINE) {
		consume();
		type = VarType::Const;
	}
	auto variable_declarations = parse_param_list(token::ASSIGN);
	if(variable_declarations.is_error()) {
		return Result<std::unique_ptr<NodeAST>>(variable_declarations.get_error());
	}
	auto variables = std::move(variable_declarations.unwrap());
	for(auto &var: variables->params) {
		auto var_ptr = dynamic_cast<NodeVariable*>(var.get());
		auto arr_ptr = dynamic_cast<NodeArray*>(var.get());
		if(var_ptr) {
			// var ist ein NodeVariable
			var_ptr->var_type = type;
		} else if(arr_ptr) {
			// var ist ein NodeArray
			std::swap(arr_ptr->sizes, arr_ptr->indexes);
		} else {
			// var ist weder ein NodeVariable noch ein NodeArray
			return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
	  "Can only declare arrays, variables or constants.", peek().line, "array or variable"));
		}
	}
	std::unique_ptr<NodeParamList> assignees;
	// if there is an assignment following
	if (peek().type == token::ASSIGN) {
		auto assignee = _parse_assignee();
		if(assignee.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(assignee.get_error());
		}
		assignees = std::move(assignee.unwrap());
	}
	// initializes empty param list
	assignees = std::make_unique<NodeParamList>();
	auto return_value = std::make_unique<NodeDeclareStatement>(std::move(variables), std::move(assignees));
	return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
}







