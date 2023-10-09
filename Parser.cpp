//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"
#include "ASTVisitor.h"

#include <filesystem>
#include <utility>


Parser::Parser(std::vector<Token> tokens): m_tokens(std::move(tokens)) {
	m_pos = 0;
	m_curr_token = m_tokens.at(0).type;
}

void Parser::parse() {
	ASTPrinter printer;
	auto prog = parse_program();
	if (prog.is_error())
		prog.get_error().print();
	else
		prog.unwrap()->accept(printer);
}

Token& Parser::get_tok() {
    return m_tokens.at(m_pos);
}

size_t Parser::get_current_pos() const {
    return m_pos;
}

void Parser::set_current_pos(size_t mPos) {
    m_pos = mPos;
}

Token Parser::peek(int ahead) {
	if (m_tokens.size() < m_pos+ahead) {
        auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
        CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
	m_curr_token = m_tokens.at(m_pos).type;
	return m_tokens.at(m_pos+ahead);

}

Token Parser::consume() {
    if (m_pos < m_tokens.size()) {
		m_curr_token = m_tokens.at(m_pos + 1).type;
        return m_tokens.at(m_pos++);
    }
    auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
    CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
    exit(EXIT_FAILURE);
}

void Parser::_skip_linebreaks() {
    while(peek().type == token::LINEBRK){
        consume();
    }
}

std::string Parser::sanitize_binary(const std::string& input) {
    if (input.empty()) {
        return input;
    }
    std::string sanitized = input;
    // Entferne das 'b' oder 'B' am Anfang oder Ende und kehre die Zeichenfolge um, falls nötig
    if (input[0] == 'b' || input[0] == 'B') {
        sanitized = input.substr(1);
        std::reverse(sanitized.begin(), sanitized.end());
    } else if (input.back() == 'b' || input.back() == 'B') {
        sanitized = input.substr(0, input.size() - 1);
    }
    return sanitized;
}

std::string Parser::sanitize_hex(const std::string& input) {
    // Überprüfen, ob der String mit einer Ziffer zwischen 0 und 9 beginnt und mit "h" endet
    if (input.size() > 1 && isdigit(input[0]) && input.back() == 'h') {
        // Entfernen der ersten Ziffer und des letzten "h"
        std::string newStr = input.substr(1, input.size() - 2);
        // Hinzufügen von "0x" am Anfang
        return newStr;
    }
    // Wenn die Bedingungen nicht erfüllt sind, den ursprünglichen String zurückgeben
    return input;
}

Result<std::unique_ptr<NodeInt>> Parser::parse_int(const Token& tok, int base) {
    auto value = tok.val;
    if(base == 2)
        value = sanitize_binary(value);
    else if (base == 16) {
        value = sanitize_hex(value);
    }
    try {
        long long val = std::stoll(value, nullptr, base);
        auto node = std::make_unique<NodeInt>(static_cast<int32_t>(val & 0xFFFFFFFF), get_tok());
        node->type = ASTType::Integer;
        return Result<std::unique_ptr<NodeInt>>(std::move(node));
    } catch (const std::exception& e) {
        auto expected = std::string(1, "valid int base "[base]);
        return Result<std::unique_ptr<NodeInt>>(
            CompileError(ErrorType::ParseError, "Invalid integer format.", tok.line, expected, value, tok.file));
    }
}


Result<std::unique_ptr<NodeString>> Parser::parse_string() {
    auto return_value = std::make_unique<NodeString>(std::move(consume().val), get_tok());
    return Result<std::unique_ptr<NodeString>>(std::move(return_value));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_number() {
    auto value = consume(); // consume int/float/hexa/binary
    std::unique_ptr<NodeAST> number_node = nullptr;
    // convert with c methods, error catching
    if (value.type == token::INT) {
        auto parsed_int = parse_int(value, 10);
        if(parsed_int.is_error())
            return Result<std::unique_ptr<NodeAST>>(parsed_int.get_error());
        return Result<std::unique_ptr<NodeAST>>(std::move(parsed_int.unwrap()));
    } else if(value.type == token::FLOAT) {
        try {
            auto node = std::make_unique<NodeReal>(std::stod(value.val), get_tok());
            node->type = ASTType::Real;
            return Result<std::unique_ptr<NodeAST>>(std::move(node));
        } catch (const std::exception &e) {
            return Result<std::unique_ptr<NodeAST>>(
                    CompileError(ErrorType::ParseError, "Invalid real format.", value.line, "valid real", value.val, value.file));
        }
    } else if(value.type == token::HEXADECIMAL) {
        auto parsed_hex = parse_int(value, 16);
        if(parsed_hex.is_error())
            return Result<std::unique_ptr<NodeAST>>(parsed_hex.get_error());
        return Result<std::unique_ptr<NodeAST>>(std::move(parsed_hex.unwrap()));
    } else if (value.type == token::BINARY) {
        auto parsed_binary = parse_int(value, 2);
        if(parsed_binary.is_error())
            return Result<std::unique_ptr<NodeAST>>(parsed_binary.get_error());
        return Result<std::unique_ptr<NodeAST>>(std::move(parsed_binary.unwrap()));
    }
    return Result<std::unique_ptr<NodeAST>>(
    CompileError(ErrorType::ParseError, "Unknown number type.", value.line, "INT, REAL, HEXADECIMAL, BINARY", value.val, value.file));
}

Result<std::unique_ptr<NodeVariable>> Parser::parse_variable(bool is_persistent, VarType var_type) {
    // see if variable already has identifier
//    char ident = 0;
//    std::string var_name = peek().val;
//    if (contains(VAR_IDENT, peek().val[0])) {
//        ident = peek().val[0];
//        var_name = peek().val.substr(1);
//    }
    auto var_name = consume().val;
    auto return_value = std::make_unique<NodeVariable>(is_persistent, var_name, var_type, get_tok());
    return Result<std::unique_ptr<NodeVariable>>(std::move(return_value));
}

Result<std::unique_ptr<NodeArray>> Parser::parse_array(bool is_persistent, VarType var_type) {
    std::unique_ptr<NodeParamList> indexes;
	std::unique_ptr<NodeParamList> sizes;
    // see if variable already has identifier
    auto arr_name = consume().val;
    if(peek().type == token::OPEN_BRACKET) {
        auto index_params = parse_param_list();
        if(index_params.is_error()) {
            return Result<std::unique_ptr<NodeArray>>(index_params.get_error());
        }
		sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
        indexes = std::move(index_params.unwrap());
    } else {
        return Result<std::unique_ptr<NodeArray>>(CompileError(ErrorType::SyntaxError,
             "Found unknown Array Syntax.", peek().line, "[", peek().val, peek().file));
    }
    auto return_value = std::make_unique<NodeArray>(is_persistent, arr_name, var_type, std::move(sizes), std::move(indexes), get_tok());
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
        lhs = std::make_unique<NodeBinaryExpr>(string_op.val, std::move(lhs), std::move(rhs.unwrap()), get_tok());
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
        std::unique_ptr<NodeAST> stmt = nullptr;
        // is function
		if (peek(1).type == token::OPEN_PARENTH) {
			auto var_function = parse_function_header();
			if (!var_function.is_error()) {
				return Result<std::unique_ptr<NodeAST>>(std::move(var_function.unwrap()));
			}
			return Result<std::unique_ptr<NodeAST>>(var_function.get_error());
		} else if(peek(1).type == token::OPEN_BRACKET) {
            auto var_array = parse_array();
            if (var_array.is_error()) {
                return Result<std::unique_ptr<NodeAST>>(var_array.get_error());
            }
            if(peek().type == token::ARROW) {
                auto get_control = parse_get_control_statement(std::move(var_array.unwrap()));
                if (get_control.is_error())
                    return Result<std::unique_ptr<NodeAST>>(get_control.get_error());
                stmt = std::move(std::move(get_control.unwrap()));
            } else stmt = std::move(std::move(var_array.unwrap()));
            return Result<std::unique_ptr<NodeAST>>(std::move(stmt));
        } else {
            // is variable
            auto var = parse_variable();
            if (var.is_error()) {
                return Result<std::unique_ptr<NodeAST>>(var.get_error());
            }
            if(peek().type == token::ARROW) {
                auto get_control = parse_get_control_statement(std::move(var.unwrap()));
                if (get_control.is_error())
                    return Result<std::unique_ptr<NodeAST>>(get_control.get_error());
                stmt = std::move(std::move(get_control.unwrap()));
            } else stmt = std::move(std::move(var.unwrap()));
            return Result<std::unique_ptr<NodeAST>>(std::move(stmt));
        }
    // is expression in brackets
    } else if (peek().type == token::OPEN_PARENTH) {
        return _parse_parenth_expr();
    } else if (peek().type == token::INT || peek().type == token::FLOAT || peek().type == token::HEXADECIMAL || peek().type == token::BINARY) {
        return parse_number();
    // unary operators bool_not, bit_not, sub
    } else if (is_unary_operator(peek().type)){
        return parse_unary_expr();
    } else {
        return Result<std::unique_ptr<NodeAST>>(
                CompileError(ErrorType::ParseError,
                     "Found unknown expression token.", peek().line, "keyword, integer, parenthesis", peek().val, peek().file));
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_unary_expr() {
    Token unary_op = consume();
    auto expr = parse_binary_expr();
    if(expr.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    }
    auto return_value = std::make_unique<NodeUnaryExpr>(unary_op, std::move(expr.unwrap()), get_tok());
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
                 "Nested Comparisons are not allowed.", peek().line, "valid expression operator", bin_op.val, peek().file));
            }
            type = ASTType::Comparison;
		} else if (bin_op.type == token::BOOL_AND || bin_op.type == token::BOOL_OR){
			type = ASTType::Boolean;
            if (not(lhs->type == ASTType::Comparison && rhs.unwrap()->type == ASTType::Comparison)) {
                return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
                 "Boolean Operators can only connect Comparisons.", peek().line, "Comparisons", "", peek().file));
            }
//		} else if (bin_op.type == token::COMMA) {
//			type = ASTType::ParamList;
		}
        // brauch ich das jetzt schon, oder vllt erst nachher beim typisierungs-check?
        if (lhs->type == Integer && rhs.unwrap()->type == Real || lhs->type == Real && rhs.unwrap()->type == Integer) {
            return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
             "Merging of different Expression Types is not allowed.", peek().line, "One Expression Type per Expression", bin_op.val, peek().file));
        }
        lhs = std::make_unique<NodeBinaryExpr>(bin_op.val, std::move(lhs), std::move(rhs.unwrap()), get_tok());
        lhs->type = type;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_parenth_expr() {
    consume(); // eat (
    auto expr = parse_binary_expr();
    if (peek().type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::ParseError,
		 "Missing parenthesis.", peek().line, ")", peek().val, peek().file));
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
		auto return_value = std::make_unique<NodeAssignStatement>(std::move(vars), std::move(assignee.unwrap()), get_tok());
		return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
        "Found invalid Assign Statement Syntax.", peek().line, ":=", peek().val, peek().file));
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
        } else if ((peek().type == token::CALL) xor
                   (peek(1).type == token::OPEN_PARENTH or peek(1).type == token::LINEBRK)) {
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
    } else if (peek().type == token::CONST || peek().type == token::FAMILY || peek().type == token::STRUCT) {
        auto construct_stmt = parse_const_struct_family_statement();
        if(construct_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(construct_stmt.get_error());
        }
        stmt = std::move(construct_stmt.unwrap());
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
         "Found invalid Statement Syntax.", peek().line, "Statement", peek().val, peek().file));
    }
	if(peek().type == token::LINEBRK) {
		consume();
        _skip_linebreaks();
        auto value = std::make_unique<NodeStatement>(std::move(stmt), get_tok());
		return Result<std::unique_ptr<NodeStatement>>(std::move(value));
	} else {
		return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
         "Found incorrect statement syntax.", peek().line, "", peek().val, peek().file));
	}
}


Result<std::unique_ptr<NodeCallback>> Parser::parse_callback() {
    std::string begin_callback = consume().val;
    if(peek().type == token::LINEBRK) {
        consume(); // Consume LINEBREAK
        _skip_linebreaks();
        std::vector<std::unique_ptr<NodeStatement>> stmts;
        while (peek().type != token::END_CALLBACK) {
            if (peek().type == token::BEGIN_CALLBACK) {
                return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
                 "", peek().line, "end on", peek().val, peek().file));
            }
            auto stmt = parse_statement();
            if (!stmt.is_error()) {
                stmts.push_back(std::move(stmt.unwrap()));
            } else {
                return Result<std::unique_ptr<NodeCallback>>(stmt.get_error());
            }
        }
        std::string end_callback = consume().val; // Consume END_CALLBACK
        auto value = std::make_unique<NodeCallback>(begin_callback,std::move(stmts), end_callback, get_tok());
        return Result<std::unique_ptr<NodeCallback>>(std::move(value));
    } else {
        return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
         "", peek().line, "linebreak", peek().val, peek().file));
    }
}

Result<std::unique_ptr<NodeProgram>> Parser::parse_program() {
    std::vector<std::unique_ptr<NodeImport>> imports;
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
    std::vector<std::unique_ptr<NodeMacroDefinition>> macro_definitions;
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
		} else if (peek().type == token::DEFINE) {
			auto define_stmt = parse_define_statement();
			if (define_stmt.is_error())
				return Result<std::unique_ptr<NodeProgram>>(define_stmt.get_error());
			defines.push_back(std::move(define_stmt.unwrap()));
        } else {
            return Result<std::unique_ptr<NodeProgram>>(CompileError(ErrorType::ParseError,
             "", peek().line, "import, define, callback, function, macro", peek().val, peek().file));
        }
        _skip_linebreaks();
    }
    auto value = std::make_unique<NodeProgram>(std::move(callbacks), std::move(function_definitions), std::move(imports), std::move(macro_definitions), std::move(defines), get_tok());
    return Result<std::unique_ptr<NodeProgram>>(std::move(value));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_param_list() {
    std::unique_ptr<NodeParamList> params;
    if(peek().type == token::OPEN_BRACKET)
        consume();
    auto param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    auto result = _parse_into_param_list(param_list->params);
    if (result.is_error()) {
        return Result<std::unique_ptr<NodeParamList>>(result.get_error());
    }
    if(peek().type == token::CLOSED_BRACKET)
        consume();

    return Result<std::unique_ptr<NodeParamList>>(std::move(param_list));
}

Result<SuccessTag> Parser::_parse_into_param_list(std::vector<std::unique_ptr<NodeAST>>& params) {
    while (true) {
        if (peek().type == token::OPEN_PARENTH) {
            size_t backup_pos = m_pos; // backup token index
            auto exprResult = parse_expression();
            if (!exprResult.is_error()) {
                params.push_back(std::move(exprResult.unwrap()));
            } else {
                m_pos = backup_pos; // set back token index
                consume(); // consume (
                auto nested_param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
                auto nestedResult = _parse_into_param_list(nested_param_list->params);
                if (nestedResult.is_error()) {
                    return nestedResult;
                }
                params.push_back(std::move(nested_param_list));
                if (peek().type == token::CLOSED_PARENTH) consume(); // consume )
            }
        } else {
            auto exprResult = parse_expression();
            if (exprResult.is_error()) {
                return Result<SuccessTag>(exprResult.get_error());
            }
            params.push_back(std::move(exprResult.unwrap()));
        }
        if (peek().type != token::COMMA) break;
        consume(); // consume comma
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header() {
    std::string func_name;
    std::unique_ptr<NodeParamList> func_args = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
	func_name = consume().val;
	if (peek().type == token::OPEN_PARENTH) {
        consume(); // consume (
		if(peek().type != token::CLOSED_PARENTH) {
            auto param_list = parse_param_list();
            if (param_list.is_error()) {
                Result<std::unique_ptr<NodeFunctionHeader>>(param_list.get_error());
            }
            func_args = std::move(param_list.unwrap());
        }
	}
    if (peek().type == token::CLOSED_PARENTH) {
        // consume both parentheses and add empty vector as Param List
        consume();
    }
    auto value = std::make_unique<NodeFunctionHeader>(func_name, std::move(func_args), get_tok());
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
    auto return_value = std::make_unique<NodeFunctionCall>(is_call, std::move(func_stmt.unwrap()), get_tok());
    return Result<std::unique_ptr<NodeFunctionCall>>(std::move(return_value));
}


Result<std::unique_ptr<NodeFunctionDefinition>> Parser::parse_function_definition() {
    std::unique_ptr<NodeFunctionHeader> func_header;
    std::optional<std::unique_ptr<NodeVariable>> func_return_var;
    std::vector<std::unique_ptr<NodeStatement>> func_body;
    bool func_override = false;
    consume(); //consume "function"
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
        "Missing function name.",peek().line,"keyword",peek().val, peek().file));
    }
    auto header = parse_function_header();
    if (header.is_error()) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(header.get_error());
    }
    func_header = std::move(header.unwrap());
    func_return_var = {};
    if (peek().type == token::ARROW) {
        consume();
        if (peek().type == token::KEYWORD) {
            auto return_var = parse_variable();
            if (return_var.is_error()) {
                Result<std::unique_ptr<NodeFunctionDefinition>>(return_var.get_error());
            }
            func_return_var = std::move(return_var.unwrap());
        } else {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::ParseError,
             "Missing return variable after ->",peek().line,"return variable",peek().val, peek().file));
        }
    }
    if (peek().type == token::OVERRIDE) {
        consume();
        func_override = true;
    }
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
        "Missing necessary linebreak after function header.",peek().line,"linebreak",peek().val, peek().file));
    }
    while (peek().type != token::END_FUNCTION) {
        auto stmt = parse_statement();
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(stmt.get_error());
        }
        func_body.push_back(std::move(stmt.unwrap()));
    }
    consume();
    auto value = std::make_unique<NodeFunctionDefinition>(std::move(func_header), std::move(func_return_var), func_override, std::move(func_body), get_tok());
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(value));
}

Result<std::unique_ptr<NodeDeclareStatement>> Parser::parse_declare_statement() {
    consume(); //consume declare
    std::unique_ptr<NodeParamList> to_be_declared = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    if(not(peek().type == KEYWORD or peek().type == UI_CONTROL or peek().type ==READ or peek().type==CONST or peek().type ==POLYPHONIC or peek().type==LOCAL or peek().type==GLOBAL))
        return Result<std::unique_ptr<NodeDeclareStatement>>(CompileError(ErrorType::ParseError,
        "Incorrect syntax in declare statement.",peek().line,"variable name",peek().val, peek().file));
    do {
        if(peek().type == token::COMMA) consume();
        // ui_control
        if (peek().type == token::UI_CONTROL xor peek(1).type == token::UI_CONTROL) {
            auto parsed_ui_control = parse_declare_ui_control();
            if (parsed_ui_control.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_ui_control.get_error());
            }
            to_be_declared->params.push_back(std::move(parsed_ui_control.unwrap()));
        // array
        } else if (is_array_declaration()) {
            auto parsed_arr = parse_declare_array();
            if(parsed_arr.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_arr.get_error());
            }
            to_be_declared->params.push_back(std::move(parsed_arr.unwrap()));
        // variable
        } else if(is_variable_declaration()) {
            auto parsed_var = parse_declare_variable();
            if (parsed_var.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_var.get_error());
            }
            to_be_declared->params.push_back(std::move(parsed_var.unwrap()));
        }
    } while(peek().type == token::COMMA);

	std::unique_ptr<NodeParamList> assignees = nullptr;
	// if there is an assignment following
	if (peek().type == token::ASSIGN) {
        consume(); //consume :=
		auto assignee = parse_param_list();
		if(assignee.is_error()) {
			return Result<std::unique_ptr<NodeDeclareStatement>>(assignee.get_error());
		}
		assignees = std::move(assignee.unwrap());
	} else
	    // initializes empty param list
	    assignees = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));

	auto return_value = std::make_unique<NodeDeclareStatement>(std::move(to_be_declared), std::move(assignees), get_tok());
	return Result<std::unique_ptr<NodeDeclareStatement>>(std::move(return_value));
}

bool Parser::is_variable_declaration() {
    // read local (const | polyphonic) keyword
    bool first = peek().type == READ and (peek(1).type == LOCAL or peek(1).type == GLOBAL) and (peek(2).type == CONST || peek(2).type == POLYPHONIC) and peek(3).type == KEYWORD;
    // read (const | polyphonic) keyword
    bool second = peek().type == READ and (peek(1).type == CONST || peek(1).type == POLYPHONIC) and peek(2).type == KEYWORD;
    // read keyword
    bool third = peek().type == READ and peek(1).type == KEYWORD;
    // read local keyword
    bool sixth = peek().type == READ and (peek(1).type == LOCAL or peek(1).type == GLOBAL) and peek(2).type == KEYWORD;
    // (const | polyphonic) keyword
    bool fifth = (peek().type == CONST || peek().type == POLYPHONIC) and peek(1).type == KEYWORD;
    // local keyword
    bool seventh = (peek().type == LOCAL or peek().type == GLOBAL) and peek(1).type == KEYWORD;
	// local (const | polyphonic) keyword
	bool eighth = (peek().type == LOCAL or peek().type == GLOBAL) and (peek(1).type == CONST || peek(1).type == POLYPHONIC) and peek(2).type == KEYWORD;
    // keyword
    bool fourth = peek().type == KEYWORD;

    return first xor second xor third xor fourth xor fifth xor sixth xor seventh xor eighth;
}

bool Parser::is_array_declaration() {
    // read local a[]
    bool first = peek().type == READ and (peek(1).type == LOCAL or peek(1).type == GLOBAL) and peek(2).type == KEYWORD and peek(3).type == OPEN_BRACKET;
    // local a[]
    bool second = (peek().type == LOCAL or peek().type == GLOBAL) and peek(1).type == KEYWORD and peek(2).type == OPEN_BRACKET;
    // read a[]
    bool third = peek().type == READ and peek(1).type == KEYWORD and peek(2).type == OPEN_BRACKET;
    // a[]
    bool fourth = peek().type == KEYWORD and peek(1).type == OPEN_BRACKET;

    return first xor second xor third xor fourth;
}


Result<std::unique_ptr<NodeVariable>> Parser::parse_declare_variable() {
    bool is_persistent = false;
    if(peek().type == token::READ) {
        is_persistent = true;
        consume();
    }
    bool is_local = false;
    if(peek().type == token::LOCAL or peek().type == token::GLOBAL) {
        is_local = peek().type == token::LOCAL;
        consume();
    }
    VarType var_type = VarType::Mutable;
    if(peek().type == token::CONST || peek().type == token::POLYPHONIC) {
        if(peek().type == token::CONST)
            var_type = VarType::Const;
        else if (peek().type == token::POLYPHONIC)
            var_type = VarType::Polyphonic;
        consume();
    }
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeVariable>>(CompileError(ErrorType::SyntaxError,
            "Found unknown variable declaration syntax.", peek().line, "variable keyword", peek().val, peek().file));
    }
    auto parsed_var = parse_variable(is_persistent, var_type);
    if(parsed_var.is_error()) {
        return Result<std::unique_ptr<NodeVariable>>(parsed_var.get_error());
    }
    return Result<std::unique_ptr<NodeVariable>>(std::move(parsed_var.unwrap()));
}

Result<std::unique_ptr<NodeArray>> Parser::parse_declare_array() {
    bool is_persistent = false;
    if(peek().type == token::READ) {
        is_persistent = true;
        consume();
    }
    bool is_local = false;
    if(peek().type == token::LOCAL or peek().type == token::GLOBAL) {
        is_local = peek().type == token::LOCAL;
        consume();
    }
    VarType var_type = VarType::Array;
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeArray>>(CompileError(ErrorType::SyntaxError,
         "Found unknown array declaration syntax.", peek().line, "array keyword", peek().val, peek().file));
    }
    auto parsed_arr = parse_array(is_persistent, var_type);
    if(parsed_arr.is_error()) {
        return Result<std::unique_ptr<NodeArray>>(parsed_arr.get_error());
    }
    auto array = std::move(parsed_arr.unwrap());
    std::swap(array->indexes, array->sizes);
    return Result<std::unique_ptr<NodeArray>>(std::move(array));
}

Result<std::unique_ptr<NodeUIControl>> Parser::parse_declare_ui_control() {
    bool is_persistent = false;
    if(peek().type == token::READ) {
        is_persistent = true;
        consume();
    }
    VarType var_type = VarType::UI_Control;
    if(peek().type != token::UI_CONTROL) {
        return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::SyntaxError,
   "Found unknown ui_control declaration syntax.", peek().line, "valid ui_control type", peek().val, peek().file));
    }
    std::string ui_control_type = consume().val;
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::SyntaxError,
   "Found unknown ui_control declaration syntax.", peek().line, "array or variable keyword", peek().val, peek().file));
    }
    std::unique_ptr<NodeAST> control_var;
    if(peek(1).type == token::OPEN_BRACKET) {
        auto parsed_arr = parse_array(is_persistent, var_type);
        if(parsed_arr.is_error()) {
            return Result<std::unique_ptr<NodeUIControl>>(parsed_arr.get_error());
        }
        auto array = std::move(parsed_arr.unwrap());
        std::swap(array->indexes, array->sizes);
        control_var = std::move(array);
    } else {
        auto parsed_var = parse_variable(is_persistent, var_type);
        if(parsed_var.is_error()) {
            return Result<std::unique_ptr<NodeUIControl>>(parsed_var.get_error());
        }
        control_var = std::move(parsed_var.unwrap());
    }
    std::unique_ptr<NodeParamList> control_params;
    if(peek().type == token::OPEN_PARENTH) {
        auto param_list = parse_param_list();
        if (param_list.is_error()) {
            Result<std::unique_ptr<NodeUIControl>>(param_list.get_error());
        }
        control_params = std::move(param_list.unwrap());
    } else {
        control_params = std::unique_ptr<NodeParamList>();
    }
    auto result = std::make_unique<NodeUIControl>(std::move(ui_control_type), std::move(control_var), std::move(control_params), get_tok());
    return Result<std::unique_ptr<NodeUIControl>>(std::move(result));
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
        "If Statement needs condition.", peek().line, "condition", "", peek().file));
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
         "Expected linebreak after if-condition.", peek().line, "linebreak", peek().val, peek().file));
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
            "Expected linebreak after else-condition.", peek().line, "linebreak", peek().val, peek().file));
        }
        if(peek().type == token::IF) {
            no_end_if = true;
            auto stmt = parse_if_statement();
            if (stmt.is_error()) {
                return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
            }
            auto stmt_val = std::make_unique<NodeStatement>(std::move(stmt.unwrap()), get_tok());
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
        auto return_value = std::make_unique<NodeIfStatement>(std::move(condition), std::move(if_stmts), std::move(else_stmts), get_tok());
        return Result<std::unique_ptr<NodeIfStatement>>(std::move(return_value));
    } else {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
         "Missing end token.", peek().line, "end if", peek().val, peek().file));
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
        "Incorrect for loop Syntax.", peek().line, "to/downto", peek().val, peek().file));
    }
    Token to = consume(); //consume to or downto
    auto expression_stmt = parse_binary_expr();
    if(expression_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForStatement>>(expression_stmt.get_error());
    }
    auto iterator_end = std::move(expression_stmt.unwrap());
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeForStatement>>(CompileError(ErrorType::SyntaxError,
         "Missing linebreak in for loop", peek().line, "linebreak", peek().val, peek().file));
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

//	std::vector<std::unique_ptr<NodeAST>> iterator_param_list;
//	iterator_param_list.push_back(std::move(iterator));
//	auto args = std::make_unique<NodeParamList>(std::move(iterator_param_list));
//	auto inc_func_header = std::make_unique<NodeFunctionHeader>("inc", std::move(args));
//	auto inc_func_call = std::make_unique<NodeFunctionCall>(false, std::move(inc_func_header));
//	auto inc_func_stmt = std::make_unique<NodeStatement>(std::move(inc_func_call));
//	stmts.push_back(std::move(inc_func_stmt));
    auto return_value = std::make_unique<NodeForStatement>(std::move(iterator), to, std::move(iterator_end), std::move(stmts), get_tok());
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
         "While Statement needs condition.", peek().line, "condition", "", peek().file));
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeWhileStatement>>(CompileError(ErrorType::SyntaxError,
         "Expected linebreak after while-condition.", peek().line, "linebreak", peek().val, peek().file));
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
    auto return_value = std::make_unique<NodeWhileStatement>(std::move(condition), std::move(stmts), get_tok());
    return Result<std::unique_ptr<NodeWhileStatement>>(std::move(return_value));
}

Result<std::unique_ptr<NodeSelectStatement>> Parser::parse_select_statement() {
	consume(); //consume select
	auto expression = parse_expression();
	if(peek().type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		"Expected linebreak after select-expression.", peek().line, "linebreak", peek().val, peek().file));
	}
	consume(); //consume linebreak
	if(peek().type != token::CASE) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		 "Expected cases in select-expression.", peek().line, "case <expression>", peek().val, peek().file));
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
				 "Expected linebreak after case.", peek().line, "linebreak", peek().val, peek().file));
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
	auto return_value = std::make_unique<NodeSelectStatement>(std::move(expression.unwrap()), std::move(cases), get_tok());
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
		 "Defines need to have expressions assigned.", peek().line, ":=", peek().val, peek().file));
	}
	consume(); // consume assign :=
	auto assignees = parse_param_list();
	if(assignees.is_error()) {
		return Result<std::unique_ptr<NodeDefineStatement>>(assignees.get_error());
	}
	auto return_value = std::make_unique<NodeDefineStatement>(std::move(definitions.unwrap()), std::move(assignees.unwrap()), get_tok());
	return Result<std::unique_ptr<NodeDefineStatement>>(std::move(return_value));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_const_struct_family_statement() {
    Token construct = consume(); //consume family, struct, const
    token end_construct = token::END_CONST;
    if (construct.type == token::FAMILY) {
        end_construct = token::END_FAMILY;
    } else if (construct.type == token::STRUCT) {
        end_construct = token::END_STRUCT;
    }

    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
            "Found unknown const/family/struct syntax.", peek().line, "valid identifier", peek().val, peek().file));
    }
    auto prefix = consume(); //consume prefix
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
            "Expected linebreak.", peek().line, "linebreak", peek().val, peek().file));
    }
    consume(); // consume linebreak
    std::vector<std::unique_ptr<NodeDeclareStatement>> stmts;
    while(peek().type != end_construct) {
        _skip_linebreaks();
        auto declare_stmt = parse_declare_statement();
        if(declare_stmt.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(declare_stmt.get_error());
        }
        stmts.push_back(std::move(declare_stmt.unwrap()));
        if(peek().type != token::LINEBRK) {
            return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
               "Missing linebreak after statement.", peek().line, "linebreak", peek().val, peek().file));
        }
        consume();
    }
    consume();
    std::unique_ptr<NodeAST> return_value = nullptr;
    if (construct.type == token::CONST)
        return_value = std::make_unique<NodeConstStatement>(std::move(prefix.val), std::move(stmts), get_tok());
    else if (construct.type == token::FAMILY)
        return_value = std::make_unique<NodeFamilyStatement>(std::move(prefix.val), std::move(stmts), get_tok());
    else if (construct.type == token::STRUCT)
        return_value = std::make_unique<NodeStructStatement>(std::move(prefix.val), std::move(stmts), get_tok());
    return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
}

Result<std::unique_ptr<NodeGetControlStatement>> Parser::parse_get_control_statement(std::unique_ptr<NodeAST> ui_id) {
//    std::unique_ptr<NodeAST> ui_id;
//    if(peek(1).type == token::OPEN_BRACKET) {
//        auto ui_control = parse_array();
//        if(ui_control.is_error())
//            return Result<std::unique_ptr<NodeGetControlStatement>>(ui_control.get_error());
//        ui_id = std::move(ui_control.unwrap());
//    } else {
//        auto ui_control = parse_variable();
//        if(ui_control.is_error())
//            return Result<std::unique_ptr<NodeGetControlStatement>>(ui_control.get_error());
//        ui_id = std::move(ui_control.unwrap());
//    }
    if(peek().type != token::ARROW) {
        return Result<std::unique_ptr<NodeGetControlStatement>>(CompileError(ErrorType::SyntaxError,
     "Wrong control statement syntax.", peek().line, "->", peek().val, peek().file));
    }
    consume(); // consume ->
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeGetControlStatement>>(CompileError(ErrorType::SyntaxError,
         "Wrong control statement syntax.", peek().line, "<control_parameter>", peek().val, peek().file));
    }
    auto control_param = consume().val;
    auto result = std::make_unique<NodeGetControlStatement>(std::move(ui_id), control_param, get_tok());
    return Result<std::unique_ptr<NodeGetControlStatement>>(std::move(result));
}






