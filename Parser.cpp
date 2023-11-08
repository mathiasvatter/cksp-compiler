//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"
#include "AST/ASTVisitor.h"

#include <filesystem>
#include <utility>


Parser::Parser(std::vector<Token> tokens, std::vector<std::string> macro_definitions): m_tokens(std::move(tokens)) {
    m_macro_definitions = std::move(macro_definitions);
	m_pos = 0;
	m_curr_token = m_tokens.at(0).type;
}

Result<std::unique_ptr<NodeProgram>> Parser::parse() {
	return parse_program();

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
        auto err_msg = "Reached the end of the m_tokens. Wrong Syntax discovered.";
        CompileError(ErrorType::ParseError, err_msg, m_tokens.at(m_pos).line, "end token", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
	m_curr_token = m_tokens.at(m_pos).type;
    m_curr_token_value = m_tokens.at(m_pos).val;
	return m_tokens.at(m_pos+ahead);

}

Token Parser::consume() {
    if (m_pos < m_tokens.size()) {
		m_curr_token = m_tokens.at(m_pos + 1).type;
        return m_tokens.at(m_pos++);
    }
    auto err_msg = "Reached the end of the m_tokens. Wrong Syntax discovered.";
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

Result<std::unique_ptr<NodeInt>> Parser::parse_int(const Token& tok, int base, NodeAST* parent) {
    auto value = tok.val;
    if(base == 2)
        value = sanitize_binary(value);
    else if (base == 16) {
        value = sanitize_hex(value);
    }
    try {
        long long val = std::stoll(value, nullptr, base);
        auto node_int = std::make_unique<NodeInt>(static_cast<int32_t>(val & 0xFFFFFFFF), get_tok());
        node_int->parent= parent;
        return Result<std::unique_ptr<NodeInt>>(std::move(node_int));
    } catch (const std::exception& e) {
        auto expected = std::string(1, "valid int base "[base]);
        return Result<std::unique_ptr<NodeInt>>(
            CompileError(ErrorType::ParseError, "Invalid integer format.", tok.line, expected, value, tok.file));
    }
}


Result<std::unique_ptr<NodeString>> Parser::parse_string(NodeAST* parent) {
    auto node_string = std::make_unique<NodeString>(std::move(consume().val), get_tok());
    node_string->parent = parent;
    return Result<std::unique_ptr<NodeString>>(std::move(node_string));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_number(NodeAST* parent) {
    auto value = consume(); // consume int/float/hexa/binary
    std::unique_ptr<NodeAST> number_node = nullptr;
    // convert with c methods, error catching
    if (value.type == token::INT) {
        auto parsed_int = parse_int(value, 10, parent);
        if(parsed_int.is_error())
            return Result<std::unique_ptr<NodeAST>>(parsed_int.get_error());
        return Result<std::unique_ptr<NodeAST>>(std::move(parsed_int.unwrap()));
    } else if(value.type == token::FLOAT) {
        try {
            auto node_real = std::make_unique<NodeReal>(std::stod(value.val), get_tok());
            node_real->parent = parent;
            return Result<std::unique_ptr<NodeAST>>(std::move(node_real));
        } catch (const std::exception &e) {
            return Result<std::unique_ptr<NodeAST>>(
			CompileError(ErrorType::ParseError, "Invalid real format.", value.line, "valid real", value.val, value.file));
        }
    } else if(value.type == token::HEXADECIMAL) {
        auto parsed_hex = parse_int(value, 16, parent);
        if(parsed_hex.is_error())
            return Result<std::unique_ptr<NodeAST>>(parsed_hex.get_error());
        return Result<std::unique_ptr<NodeAST>>(std::move(parsed_hex.unwrap()));
    } else if (value.type == token::BINARY) {
        auto parsed_binary = parse_int(value, 2, parent);
        if(parsed_binary.is_error())
            return Result<std::unique_ptr<NodeAST>>(parsed_binary.get_error());
        return Result<std::unique_ptr<NodeAST>>(std::move(parsed_binary.unwrap()));
    }
    return Result<std::unique_ptr<NodeAST>>(
    CompileError(ErrorType::ParseError, "Unknown number type.", value.line, "INT, REAL, HEXADECIMAL, BINARY", value.val, value.file));
}

Result<std::unique_ptr<NodeVariable>> Parser::parse_variable(NodeAST* parent, bool is_persistent, VarType var_type) {
    // see if variable already has identifier
//    char ident = 0;
//    std::string var_name = peek().val;
//    if (contains(VAR_IDENT, peek().val[0])) {
//        ident = peek().val[0];
//        var_name = peek().val.substr(1);
//    }
    auto var_name = consume().val;
    auto node_variable = std::make_unique<NodeVariable>(is_persistent, var_name, var_type, get_tok());
    node_variable->parent = parent;
    return Result<std::unique_ptr<NodeVariable>>(std::move(node_variable));
}

Result<std::unique_ptr<NodeArray>> Parser::parse_array(NodeAST* parent, bool is_persistent, VarType var_type) {
    auto node_array = std::make_unique<NodeArray>(get_tok());
    std::unique_ptr<NodeParamList> indexes = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));;
    indexes->parent = node_array.get();
	std::unique_ptr<NodeParamList> sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    sizes->parent = node_array.get();
    // see if variable already has identifier
    auto arr_name = consume().val;
    if(peek().type == token::OPEN_BRACKET) {
        consume(); // consume [
        // if it is an empty array initialization
        if (peek().type != token::CLOSED_BRACKET) {
            auto index_params = parse_param_list(node_array.get());
            if (index_params.is_error()) {
                return Result<std::unique_ptr<NodeArray>>(index_params.get_error());
            }
            indexes = std::move(index_params.unwrap());
        }
        if(peek().type != token::CLOSED_BRACKET)
            return Result<std::unique_ptr<NodeArray>>(CompileError(ErrorType::SyntaxError,
           "Found unknown Array Syntax.", peek().line, "]", peek().val, peek().file));
        consume(); // consume ]
    } else {
        return Result<std::unique_ptr<NodeArray>>(CompileError(ErrorType::SyntaxError,
         "Found unknown Array Syntax.", peek().line, "[", peek().val, peek().file));
    }
    node_array->parent = parent;
    node_array->is_persistent = is_persistent;
    node_array->is_local = false;
    node_array->var_type = var_type;
    node_array->name = arr_name;
    node_array->sizes = std::move(sizes);
    node_array->indexes = std::move(indexes);
//    auto return_value = std::make_unique<NodeArray>(is_persistent, arr_name, var_type, std::move(sizes), std::move(indexes), get_tok());
    return Result<std::unique_ptr<NodeArray>>(std::move(node_array));
}


Result<std::unique_ptr<NodeAST>> Parser::parse_expression(NodeAST* parent) {
    auto lhs = parse_string_expr(parent);
    if(lhs.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(lhs.get_error());
    }
    return _parse_string_expr_rhs(std::move(lhs.unwrap()), parent);
//    return std::move(lhs);
}

Result<std::unique_ptr<NodeAST>> Parser::parse_string_expr(NodeAST* parent) {
    if (peek().type == token::STRING) {
        auto expr = parse_string(parent);
        if (!expr.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(std::move(expr.unwrap()));
        } else return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    } else {
        auto expr = parse_binary_expr(parent);
        if (!expr.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(std::move(expr.unwrap()));
        } else return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    }
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_string_expr_rhs(std::unique_ptr<NodeAST> lhs, NodeAST* parent) {
    while(true) {
        Token string_op = peek();
        if(peek().type != token::STRING_OPERATOR) {
            return Result<std::unique_ptr<NodeAST>>(std::move(lhs));
        }
        consume();
        auto rhs = parse_string_expr(lhs.get());
        if(rhs.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
        }
        rhs = _parse_string_expr_rhs(std::move(rhs.unwrap()), parent);
        if (rhs.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
        }
		auto node_binary_expr = std::make_unique<NodeBinaryExpr>(string_op.val, std::move(lhs), std::move(rhs.unwrap()), get_tok());
		node_binary_expr->left->parent = node_binary_expr.get();
		node_binary_expr->right->parent = node_binary_expr.get();
		node_binary_expr->parent = parent;
        lhs = std::move(node_binary_expr);
        lhs->type = ASTType::String;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_binary_expr(NodeAST* parent) {
    auto lhs = _parse_primary_expr(parent);
    if(!lhs.is_error()) {
        return _parse_binary_expr_rhs(0, std::move(lhs.unwrap()), parent);
    }
    return Result<std::unique_ptr<NodeAST>>(lhs.get_error());
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_primary_expr(NodeAST* parent) {
    if (peek().type == token::KEYWORD) {
        std::unique_ptr<NodeAST> stmt = nullptr;
        // is function
		if (peek(1).type == token::OPEN_PARENTH) {
			auto var_function = parse_function_call(parent);
			if (!var_function.is_error()) {
				return Result<std::unique_ptr<NodeAST>>(std::move(var_function.unwrap()));
			}
			return Result<std::unique_ptr<NodeAST>>(var_function.get_error());
		} else if(peek(1).type == token::OPEN_BRACKET) {
            auto var_array = parse_array(parent);
            if (var_array.is_error()) {
                return Result<std::unique_ptr<NodeAST>>(var_array.get_error());
            }
            if(peek().type == token::ARROW) {
                auto get_control = parse_get_control_statement(std::move(var_array.unwrap()), parent);
                if (get_control.is_error())
                    return Result<std::unique_ptr<NodeAST>>(get_control.get_error());
                stmt = std::move(std::move(get_control.unwrap()));
            } else stmt = std::move(std::move(var_array.unwrap()));
            return Result<std::unique_ptr<NodeAST>>(std::move(stmt));
        } else {
            // is variable
            auto var = parse_variable(parent);
            if (var.is_error()) {
                return Result<std::unique_ptr<NodeAST>>(var.get_error());
            }
            if(peek().type == token::ARROW) {
                auto get_control = parse_get_control_statement(std::move(var.unwrap()), parent);
                if (get_control.is_error())
                    return Result<std::unique_ptr<NodeAST>>(get_control.get_error());
                stmt = std::move(std::move(get_control.unwrap()));
            } else stmt = std::move(std::move(var.unwrap()));
            return Result<std::unique_ptr<NodeAST>>(std::move(stmt));
        }
    // is expression in brackets
    } else if (peek().type == token::OPEN_PARENTH) {
        return _parse_parenth_expr(parent);
    } else if (peek().type == token::INT || peek().type == token::FLOAT || peek().type == token::HEXADECIMAL || peek().type == token::BINARY) {
        return parse_number(parent);
    // unary operators bool_not, bit_not, sub
    } else if (is_unary_operator(peek().type)){
        return parse_unary_expr(parent);
    } else {
        return Result<std::unique_ptr<NodeAST>>(
    CompileError(ErrorType::ParseError,"Found unknown expression token.", peek().line, "keyword, integer, parenthesis", peek().val, peek().file));
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_unary_expr(NodeAST* parent) {
    auto node_unary_expr = std::make_unique<NodeUnaryExpr>(get_tok());
    Token unary_op = consume();
    auto expr = _parse_primary_expr(node_unary_expr.get());
    if(expr.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    }
    node_unary_expr->op = unary_op;
    node_unary_expr->operand = std::move(expr.unwrap());
    node_unary_expr->parent = parent;
//    auto return_value = std::make_unique<NodeUnaryExpr>(unary_op, std::move(expr.unwrap()), get_tok());
    return Result<std::unique_ptr<NodeAST>>(std::move(node_unary_expr));
}


Result<std::unique_ptr<NodeAST>> Parser::_parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs, NodeAST* parent) {
    while(true) {
        int prec = _get_binop_precedence(peek().type);
        if(prec < precedence)
            return Result<std::unique_ptr<NodeAST>>(std::move(lhs));
        // its not -1 so it is a binop
        auto bin_op = peek();
        consume();
        auto rhs = _parse_primary_expr(parent);
        if (rhs.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
        }
        int next_precedence = _get_binop_precedence(peek().type);
        if (prec < next_precedence) {
            rhs = _parse_binary_expr_rhs(prec + 1, std::move(rhs.unwrap()), parent);
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
//            if (not(lhs->type == ASTType::Comparison && rhs.unwrap()->type == ASTType::Comparison)) {
//                return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
//                 "Boolean Operators can only connect Comparisons.", peek().line, "Comparisons", "", peek().file));
//            }
		}
        // brauch ich das jetzt schon, oder vllt erst nachher beim typisierungs-check?
        if (lhs->type == Integer && rhs.unwrap()->type == Real || lhs->type == Real && rhs.unwrap()->type == Integer) {
            return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
             "Merging of different Expression Types (real and int) is not allowed. Use int(<expr>) or real(<expr>) to cast types.",
			 peek().line, "", bin_op.val, peek().file));
        }
		auto node_binary_expr = std::make_unique<NodeBinaryExpr>(bin_op.val, std::move(lhs), std::move(rhs.unwrap()), get_tok());
        node_binary_expr->parent = parent;
		node_binary_expr->left->parent = node_binary_expr.get();
		node_binary_expr->right->parent = node_binary_expr.get();
        lhs = std::move(node_binary_expr);
        lhs->type = type;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_parenth_expr(NodeAST* parent) {
    consume(); // eat (
    auto expr = parse_binary_expr(parent);
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

Result<std::unique_ptr<NodeAssignStatement>> Parser::parse_assign_statement(NodeAST* parent) {
    auto node_assign_statement = std::make_unique<NodeAssignStatement>(get_tok());
	// make it possible to have more than one variable before assign
	auto var_list = parse_param_list(node_assign_statement.get());
	if(var_list.is_error()) {
		return Result<std::unique_ptr<NodeAssignStatement>>(var_list.get_error());
	}
	auto vars = std::move(var_list.unwrap());
    if(peek().type != token::ASSIGN) {
        return Result<std::unique_ptr<NodeAssignStatement>>(CompileError(ErrorType::SyntaxError,
"Found invalid Assign Statement Syntax.", peek().line,":=", peek().val, peek().file));
    }
    consume(); // consume :=
    auto assignee =  parse_param_list(node_assign_statement.get()); //_parse_assignee();
    if(assignee.is_error()) {
        return Result<std::unique_ptr<NodeAssignStatement>>(assignee.get_error());
    }
    node_assign_statement->array_variable = std::move(vars);
    node_assign_statement->assignee = std::move(assignee.unwrap());
    node_assign_statement->parent = parent;

//    auto return_value = std::make_unique<NodeAssignStatement>(std::move(vars), std::move(assignee.unwrap()), get_tok());
    return Result<std::unique_ptr<NodeAssignStatement>>(std::move(node_assign_statement));
}

Result<std::unique_ptr<NodeStatement>> Parser::parse_statement(NodeAST* parent) {
    auto node_statement = std::make_unique<NodeStatement>(get_tok());
    _skip_linebreaks();
    std::unique_ptr<NodeAST> stmt;
    // assign statement
    if (peek().type == token::KEYWORD || peek().type == token::DECLARE || peek().type == token::CALL) {
        if (peek().type == token::DECLARE) {
            auto declare_stmt = parse_declare_statement(node_statement.get());
            if (declare_stmt.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(declare_stmt.get_error());
            }
            stmt = std::move(declare_stmt.unwrap());
        } else if (is_macro_call()) {
            auto macro_call = parse_macro_call(node_statement.get());
            if (macro_call.is_error())
                return Result<std::unique_ptr<NodeStatement>>(macro_call.get_error());
            stmt = std::move(macro_call.unwrap());
        } else if ((peek().type == token::CALL) xor
                   (peek(1).type == token::OPEN_PARENTH or peek(1).type == token::LINEBRK or peek(1).type == CLOSED_PARENTH)) {
            auto function_call = parse_function_call(node_statement.get());
            if (function_call.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(function_call.get_error());
            }
            stmt = std::move(function_call.unwrap());
        } else {
            auto assign_stmt = parse_assign_statement(node_statement.get());
            if (assign_stmt.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(assign_stmt.get_error());
            }
            stmt = std::move(assign_stmt.unwrap());
        }
    } else if (peek().type == token::MACRO) {
        auto macro = parse_macro_definition(node_statement.get());
        if (macro.is_error())
            return Result<std::unique_ptr<NodeStatement>>(macro.get_error());
        macro_definitions.push_back(std::move(macro.unwrap()));
//        stmt = std::move(macro.unwrap());
    } else if (peek().type == token::ITERATE_MACRO) {
        auto iterate_macro = parse_iterate_macro(node_statement.get());
        if (iterate_macro.is_error())
            return Result<std::unique_ptr<NodeStatement>>(iterate_macro.get_error());
        stmt = std::move(iterate_macro.unwrap());
    } else if (peek().type == token::LITERATE_MACRO) {
        auto literate_macro = parse_literate_macro(node_statement.get());
        if (literate_macro.is_error())
            return Result<std::unique_ptr<NodeStatement>>(literate_macro.get_error());
        stmt = std::move(literate_macro.unwrap());
//	} else if (peek().type == token::DEFINE) {
//		while (peek().type != token::LINEBRK) consume();
//		consume();
    } else if (peek().type == token::CONST || peek().type == token::STRUCT) {
		auto construct_stmt = parse_const_struct_family_statement(node_statement.get());
		if (construct_stmt.is_error()) {
			return Result<std::unique_ptr<NodeStatement>>(construct_stmt.get_error());
		}
		stmt = std::move(construct_stmt.unwrap());
	} else if (peek().type == token::FAMILY) {
		auto family_stmt = parse_family_statement(node_statement.get());
		if (family_stmt.is_error()) {
			return Result<std::unique_ptr<NodeStatement>>(family_stmt.get_error());
		}
		stmt = std::move(family_stmt.unwrap());
    } else if (peek().type == token::IF) {
        auto if_stmt = parse_if_statement(node_statement.get());
        if (if_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(if_stmt.get_error());
        }
        stmt = std::move(if_stmt.unwrap());
    } else if (peek().type == token::FOR) {
        auto for_stmt = parse_for_statement(node_statement.get());
        if (for_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(for_stmt.get_error());
        }
        stmt = std::move(for_stmt.unwrap());
    } else if (peek().type == token::WHILE) {
        auto while_stmt = parse_while_statement(node_statement.get());
        if(while_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(while_stmt.get_error());
        }
        stmt = std::move(while_stmt.unwrap());
	} else if (peek().type == token::SELECT) {
		auto select_stmt = parse_select_statement(node_statement.get());
		if(select_stmt.is_error()) {
			return Result<std::unique_ptr<NodeStatement>>(select_stmt.get_error());
		}
		stmt = std::move(select_stmt.unwrap());
    } else {
        return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
         "Found invalid Statement Syntax.", peek().line, "Statement", peek().val, peek().file));
    }
    if(!is_instance_of<NodeIterateMacro>(parent) and !is_instance_of<NodeLiterateMacro>(parent)) {
        if (peek().type != token::LINEBRK) {
            return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
    "Found incorrect statement syntax.", peek().line,"", peek().val, peek().file));
        }
        consume();
    }
    _skip_linebreaks();
    node_statement->statement = std::move(stmt);
    node_statement->parent = parent;
//    auto value = std::make_unique<NodeStatement>(std::move(stmt), get_tok());
    return Result<std::unique_ptr<NodeStatement>>(std::move(node_statement));
}


Result<std::unique_ptr<NodeCallback>> Parser::parse_callback(NodeAST* parent) {
    auto node_callback = std::make_unique<NodeCallback>(get_tok());
    auto node_statement_list = std::make_unique<NodeStatementList>(get_tok());
    node_statement_list->parent = node_callback.get();
    std::string begin_callback = consume().val;
    std::unique_ptr<NodeAST> callback_id;
    if(begin_callback == "on ui_control") {
        if (peek().type != token::OPEN_PARENTH) {
            return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
             "Missing open parenthesis after <callback>", peek().line, "(", peek().val, peek().file));
        }
        consume(); // consume (
        auto callback_id_result = parse_binary_expr(node_callback.get());
        if (callback_id_result.is_error()) {
            Result<std::unique_ptr<NodeFunctionHeader>>(callback_id_result.get_error());
        }
        callback_id = std::move(callback_id_result.unwrap());
        if (peek().type != token::CLOSED_PARENTH) {
            return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
              "Missing closing parenthesis after <callback>", peek().line, ")", peek().val, peek().file));
        }
        consume(); // consume )
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
      "Missing linebreak after <callback>", peek().line, "linebreak", peek().val, peek().file));
    }
    consume(); // Consume LINEBREAK
    _skip_linebreaks();
    std::vector<std::unique_ptr<NodeStatement>> stmts;
    while (peek().type != token::END_CALLBACK) {
        if (peek().type == token::BEGIN_CALLBACK) {
            return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
         "", peek().line, "end on", peek().val, peek().file));
        }
        auto stmt = parse_statement(node_statement_list.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeCallback>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            stmts.push_back(std::move(stmt.unwrap()));
    }
    node_statement_list->statements = std::move(stmts);
    std::string end_callback = consume().val; // Consume END_CALLBACK
    node_callback->begin_callback = std::move(begin_callback);
    node_callback->callback_id = std::move(callback_id);
    node_callback->statements = std::move(node_statement_list);
    node_callback->end_callback = std::move(end_callback);
    node_callback->parent = parent;
//    auto value = std::make_unique<NodeCallback>(begin_callback,std::move(stmts), end_callback, get_tok());
    return Result<std::unique_ptr<NodeCallback>>(std::move(node_callback));
}

Result<std::unique_ptr<NodeProgram>> Parser::parse_program() {
    std::vector<std::unique_ptr<NodeIterateMacro>> macro_iterations;
    std::vector<std::unique_ptr<NodeLiterateMacro>> macro_literations;
    std::vector<std::unique_ptr<NodeMacroCall>> macro_calls;
    std::vector<std::unique_ptr<NodeImport>> imports;
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> function_definitions;
	std::vector<std::unique_ptr<NodeDefineStatement>> defines;
    auto node_program = std::make_unique<NodeProgram>(get_tok());
    while (peek().type != token::END_TOKEN) {
        _skip_linebreaks();
        if (peek().type == token::BEGIN_CALLBACK) {
            auto callback = parse_callback(node_program.get());
            if (callback.is_error())
                return Result<std::unique_ptr<NodeProgram>>(callback.get_error());
//			program.push_back(std::move(callback.unwrap()));
			callbacks.push_back(std::move(callback.unwrap()));
        } else if (peek().type == token::FUNCTION) {
            auto function = parse_function_definition(node_program.get());
            if (function.is_error())
                return Result<std::unique_ptr<NodeProgram>>(function.get_error());
//            program.push_back(std::move(function.unwrap()));
			function_definitions.push_back(std::move(function.unwrap()));
		} else if (peek().type == token::DEFINE) {
            while (peek().type != token::LINEBRK) consume();
            consume();
        } else if(is_macro_call()) {
            auto macro_call = parse_macro_call(node_program.get());
            if (macro_call.is_error())
                return Result<std::unique_ptr<NodeProgram>>(macro_call.get_error());
//            macro_call_definitions.push_back(std::move(macro_call.unwrap()));
            macro_calls.push_back(std::move(macro_call.unwrap()));
		} else if (peek().type == token::MACRO) {
            auto macro = parse_macro_definition(node_program.get());
            if (macro.is_error())
                return Result<std::unique_ptr<NodeProgram>>(macro.get_error());
            macro_definitions.push_back(std::move(macro.unwrap()));
        } else if (peek().type == token::ITERATE_MACRO) {
            auto iterate_macro = parse_iterate_macro(node_program.get());
            if (iterate_macro.is_error())
                return Result<std::unique_ptr<NodeProgram>>(iterate_macro.get_error());
            macro_iterations.push_back(std::move(iterate_macro.unwrap()));
        } else if (peek().type == token::LITERATE_MACRO) {
            auto literate_macro = parse_literate_macro(node_program.get());
            if (literate_macro.is_error())
                return Result<std::unique_ptr<NodeProgram>>(literate_macro.get_error());
            macro_literations.push_back(std::move(literate_macro.unwrap()));
        } else {
            return Result<std::unique_ptr<NodeProgram>>(CompileError(ErrorType::ParseError,
             "Found unknown construct.", peek().line, "<callback>, <function_definition>", peek().val, peek().file));
        }
        auto l = consume_linebreak("<program-level construct>");
        if(l.is_error())
            return Result<std::unique_ptr<NodeProgram>>(l.get_error());
//        _skip_linebreaks();
    }
    node_program->callbacks = std::move(callbacks);
    node_program->function_definitions = std::move(function_definitions);
    node_program->macro_definitions = std::move(macro_definitions);
    node_program->imports = std::move(imports);
    node_program->defines = std::move(defines);
    node_program->macro_calls = std::move(macro_calls);
    node_program->macro_iterations = std::move(macro_iterations);
    node_program->macro_literations = std::move(macro_literations);
//    auto value = std::make_unique<NodeProgram>(std::move(callbacks), std::move(function_definitions), std::move(imports), std::move(macro_definitions), std::move(defines), get_tok());
    return Result<std::unique_ptr<NodeProgram>>(std::move(node_program));
}

Result<std::unique_ptr<NodeMacroDefinition>> Parser::parse_macro_definition(NodeAST* parent) {
    auto node_macro_definition = std::make_unique<NodeMacroDefinition>(get_tok());
    consume(); // consume macro
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
     "Missing macro name.",peek().line,"keyword",peek().val, peek().file));
    }
    auto header = parse_macro_header(node_macro_definition.get());
    if (header.is_error()) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(header.get_error());
    }
    node_macro_definition->header = std::move(header.unwrap());
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
         "Missing necessary linebreak after macro header.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak
    while (peek().type != token::END_MACRO) {
        _skip_linebreaks();
        if(peek().type == END_MACRO) break;
        if (peek().type == token::BEGIN_CALLBACK) {
            auto callback = parse_callback(node_macro_definition.get());
            if (callback.is_error())
                return Result<std::unique_ptr<NodeMacroDefinition>>(callback.get_error());
            node_macro_definition->body.push_back(std::move(callback.unwrap()));
        } else if (peek().type == token::FUNCTION) {
            auto function = parse_function_definition(node_macro_definition.get());
            if (function.is_error())
                return Result<std::unique_ptr<NodeMacroDefinition>>(function.get_error());
            node_macro_definition->body.push_back(std::move(function.unwrap()));
        } else if (peek().type == token::MACRO) {
            return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
             "Nested macros are not allowed. Maybe you forgot an 'end macro' along the m_line?",peek().line,"",peek().val, peek().file));
        } else {
            auto stmt = parse_statement(node_macro_definition.get());
            if (stmt.is_error()) {
                return Result<std::unique_ptr<NodeMacroDefinition>>(stmt.get_error());
            }
            if(stmt.unwrap()->statement)
                node_macro_definition->body.push_back(std::move(stmt.unwrap()));
        }
    }
    consume(); // consume end macro
    node_macro_definition->parent = parent;
    return Result<std::unique_ptr<NodeMacroDefinition>>(std::move(node_macro_definition));
}

Result<std::unique_ptr<NodeMacroHeader>> Parser::parse_macro_header(NodeAST* parent) {
    auto node_macro_header = std::make_unique<NodeMacroHeader>(get_tok());
    node_macro_header->parent = parent;
    auto macro_name = consume().val;
    std::unique_ptr<NodeParamList> macro_args = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    macro_args->parent=node_macro_header.get();
    if (peek().type == token::OPEN_PARENTH) {
        consume(); // consume (
        if(peek().type != token::CLOSED_PARENTH) {
            auto param_list = parse_param_list(node_macro_header.get());
            if (param_list.is_error()) {
                Result<std::unique_ptr<NodeFunctionHeader>>(param_list.get_error());
            }
            macro_args = std::move(param_list.unwrap());
        }
        if (peek().type == token::CLOSED_PARENTH) {
            // consume both parentheses and add empty vector as Param List
            consume();
        }
    }
    node_macro_header->name = std::move(macro_name);
    node_macro_header->args = std::move(macro_args);
    return Result<std::unique_ptr<NodeMacroHeader>>(std::move(node_macro_header));
}

Result<std::unique_ptr<NodeMacroCall>> Parser::parse_macro_call(NodeAST* parent) {
    auto node_macro_call = std::make_unique<NodeMacroCall>(get_tok());
    auto macro_stmt = parse_macro_header(node_macro_call.get());
    if(macro_stmt.is_error()){
        return Result<std::unique_ptr<NodeMacroCall>>(macro_stmt.get_error());
    }
    node_macro_call->macro = std::move(macro_stmt.unwrap());
    node_macro_call->parent = parent;
    return Result<std::unique_ptr<NodeMacroCall>>(std::move(node_macro_call));
}

bool Parser::is_macro_call() {
    return peek().type == KEYWORD and (peek(1).type == LINEBRK or peek(1).type == OPEN_PARENTH or peek(-1).type == OPEN_PARENTH)
            and contains(m_macro_definitions, peek().val);
}

Result<std::unique_ptr<NodeIterateMacro>> Parser::parse_iterate_macro(NodeAST* parent) {
    auto node_iterate_macro = std::make_unique<NodeIterateMacro>(get_tok());
    node_iterate_macro->parent = parent;
    consume(); // consume iterate_macro
    if(peek().type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
	  	"Found invalid <iterate_macro> statement syntax.",peek().line,"(",peek().val, peek().file));
    }
    consume(); //consume (
    auto node_statement = parse_statement(node_iterate_macro.get());
    if(node_statement.is_error())
        return Result<std::unique_ptr<NodeIterateMacro>>(node_statement.get_error());
    if(peek().type != token::CLOSED_PARENTH) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
          "Found invalid <iterate_macro> statement syntax.",peek().line,")",peek().val, peek().file));
    }
    consume(); //consume )
    if(peek().type != token::ASSIGN) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
     "Found invalid <iterate_macro> statement syntax.", peek().line,":=", peek().val, peek().file));
    }
    consume(); // consume :=
    auto iterator_start =  parse_binary_expr(node_iterate_macro.get()); //_parse_iterator_start();
    if(iterator_start.is_error()) {
        return Result<std::unique_ptr<NodeIterateMacro>>(iterator_start.get_error());
    }
    if(not(peek().type == token::TO or peek().type == token::DOWNTO)) {
        return Result<std::unique_ptr<NodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
      "Found invalid <iterate_macro> statement syntax.", peek().line,"<to>/<downto>", peek().val, peek().file));
    }
    Token to = consume(); // consume downto/to
    auto iterator_end =  parse_binary_expr(node_iterate_macro.get()); //_parse_iterator_end();
    if(iterator_end.is_error()) {
        return Result<std::unique_ptr<NodeIterateMacro>>(iterator_end.get_error());
    }
    std::unique_ptr<NodeAST> step;
    if(peek().type == STEP) {
        consume(); // consume step
        auto step_expression = parse_binary_expr(node_iterate_macro.get());
        if(step_expression.is_error())
            return Result<std::unique_ptr<NodeIterateMacro>>(step_expression.get_error());
        step = std::move(step_expression.unwrap());
    }
//    auto l = consume_linebreak("<iterate_macro>");
//    if(l.is_error())
//        return Result<std::unique_ptr<NodeIterateMacro>>(l.get_error());

    node_iterate_macro->macro_call = std::move(node_statement.unwrap());
    node_iterate_macro->iterator_start = std::move(iterator_start.unwrap());
    node_iterate_macro->iterator_end = std::move(iterator_end.unwrap());
    node_iterate_macro -> to = to;
    node_iterate_macro->step = std::move(step);
    return Result<std::unique_ptr<NodeIterateMacro>>(std::move(node_iterate_macro));
}

Result<std::unique_ptr<NodeLiterateMacro>> Parser::parse_literate_macro(NodeAST* parent) {
    auto node_literate_macro = std::make_unique<NodeLiterateMacro>(get_tok());
    node_literate_macro->parent = parent;
    consume(); // consume literate_macro
    if(peek().type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<NodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
        "Found invalid <literate_macro> statement syntax.",peek().line,"(",peek().val, peek().file));
    }
    consume(); //consume (
    auto node_statement = parse_statement(node_literate_macro.get());
    if(node_statement.is_error())
        return Result<std::unique_ptr<NodeLiterateMacro>>(node_statement.get_error());
    if(peek().type != token::CLOSED_PARENTH) {
        return Result<std::unique_ptr<NodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
        "Found invalid <literate_macro> statement syntax.",peek().line,")",peek().val, peek().file));
    }
    consume(); //consume )
    if(peek().type != token::ON) {
        return Result<std::unique_ptr<NodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
      "Found invalid <literate_macro> statement syntax.", peek().line,"on", peek().val, peek().file));
    }
    consume(); // consume on
    auto param_list = parse_param_list(node_literate_macro.get());
    if(param_list.is_error())
        return Result<std::unique_ptr<NodeLiterateMacro>>(param_list.get_error());
//    auto l = consume_linebreak("<literate_macro>");
//    if(l.is_error())
//        return Result<std::unique_ptr<NodeLiterateMacro>>(l.get_error());
    node_literate_macro->macro_call = std::move(node_statement.unwrap());
    node_literate_macro->literate_tokens = std::move(param_list.unwrap());
    return Result<std::unique_ptr<NodeLiterateMacro>>(std::move(node_literate_macro));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_param_list(NodeAST* parent) {
//    std::unique_ptr<NodeParamList> params;
    auto param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    param_list->parent = parent;
    auto result = _parse_into_param_list(param_list->params, param_list.get());
    if (result.is_error()) {
        return Result<std::unique_ptr<NodeParamList>>(result.get_error());
    }
    return Result<std::unique_ptr<NodeParamList>>(std::move(param_list));
}

Result<SuccessTag> Parser::_parse_into_param_list(std::vector<std::unique_ptr<NodeAST>>& params, NodeAST* parent) {
    while (true) {
        if (peek().type == token::OPEN_PARENTH) {
            size_t backup_pos = m_pos; // backup token index
            auto exprResult = parse_expression(parent);
            if (!exprResult.is_error()) {
                params.push_back(std::move(exprResult.unwrap()));
            } else {
                m_pos = backup_pos; // set back token index
                consume(); // consume (
                auto nested_param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
                nested_param_list->parent = parent;
                auto nestedResult = _parse_into_param_list(nested_param_list->params, nested_param_list.get());
                if (nestedResult.is_error()) {
                    return nestedResult;
                }
                params.push_back(std::move(nested_param_list));
                if (peek().type == token::CLOSED_PARENTH) consume(); // consume )
            }
        } else {
            auto exprResult = parse_expression(parent);
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

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header(NodeAST* parent) {
    auto node_function_header = std::make_unique<NodeFunctionHeader>(get_tok());
    std::string func_name;
    std::unique_ptr<NodeParamList> func_args = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    func_args->parent=node_function_header.get();
	func_name = consume().val;
	if (peek().type == token::OPEN_PARENTH) {
        consume(); // consume (
		if(peek().type != token::CLOSED_PARENTH) {
            auto param_list = parse_param_list(node_function_header.get());
            if (param_list.is_error()) {
                Result<std::unique_ptr<NodeFunctionHeader>>(param_list.get_error());
            }
            func_args = std::move(param_list.unwrap());
        }
        if (peek().type == token::CLOSED_PARENTH) {
            // consume both parentheses and add empty vector as Param List
            consume();
        }
	}
    node_function_header->name = func_name;
    node_function_header->args = std::move(func_args);
    node_function_header->parent = parent;
//    auto value = std::make_unique<NodeFunctionHeader>(func_name, std::move(func_args), get_tok());
    return Result<std::unique_ptr<NodeFunctionHeader>>(std::move(node_function_header));
}

Result<std::unique_ptr<NodeFunctionCall>> Parser::parse_function_call(NodeAST* parent) {
    auto node_function_call = std::make_unique<NodeFunctionCall>(get_tok());
    bool is_call = false;
    if(peek().type == token::CALL) {
        is_call = true;
        consume(); //consume call
    }
    auto func_stmt = parse_function_header(node_function_call.get());
    if(func_stmt.is_error()){
        return Result<std::unique_ptr<NodeFunctionCall>>(func_stmt.get_error());
    }
    node_function_call->is_call = is_call;
    node_function_call->function = std::move(func_stmt.unwrap());
    node_function_call->parent = parent;
//    auto return_value = std::make_unique<NodeFunctionCall>(is_call, std::move(func_stmt.unwrap()), get_tok());
    return Result<std::unique_ptr<NodeFunctionCall>>(std::move(node_function_call));
}


Result<std::unique_ptr<NodeFunctionDefinition>> Parser::parse_function_definition(NodeAST* parent) {
    auto node_function_definition = std::make_unique<NodeFunctionDefinition>(get_tok());
    std::unique_ptr<NodeFunctionHeader> func_header;
    std::optional<std::unique_ptr<NodeParamList>> func_return_var;
    std::vector<std::unique_ptr<NodeStatement>> func_body;
    bool func_override = false;
    consume(); //consume "function"
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
        "Missing function name.",peek().line,"keyword",peek().val, peek().file));
    }
    auto header = parse_function_header(node_function_definition.get());
    if (header.is_error()) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(header.get_error());
    }
    func_header = std::move(header.unwrap());
    func_return_var = {};
    if (peek().type == token::ARROW) {
        consume();
        if (peek().type == token::KEYWORD) {
            auto return_var = parse_param_list(parent);
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
    consume(); // consume linebreak
    while (peek().type != token::END_FUNCTION) {
        _skip_linebreaks();
        if(peek().type == END_FUNCTION) break;
        auto stmt = parse_statement(node_function_definition.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            func_body.push_back(std::move(stmt.unwrap()));
    }
    consume();
    node_function_definition->header = std::move(func_header);
    node_function_definition->return_variable = std::move(func_return_var);
    node_function_definition->override = func_override;
    node_function_definition->body = std::move(func_body);
//    auto value = std::make_unique<NodeFunctionDefinition>(std::move(func_header), std::move(func_return_var), func_override, std::move(func_body), get_tok());
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(node_function_definition));
}

Result<std::unique_ptr<NodeDeclareStatement>> Parser::parse_declare_statement(NodeAST* parent) {
    auto node_declare_statement = std::make_unique<NodeDeclareStatement>(get_tok());
    if(peek().type == DECLARE) consume(); //consume declare
    std::unique_ptr<NodeParamList> to_be_declared = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    to_be_declared->parent = node_declare_statement.get();
    if(not(peek().type == KEYWORD or peek().type == UI_CONTROL or peek().type ==READ or peek().type==CONST or peek().type ==POLYPHONIC or peek().type==LOCAL or peek().type==GLOBAL))
        return Result<std::unique_ptr<NodeDeclareStatement>>(CompileError(ErrorType::ParseError,
        "Incorrect syntax in declare statement.",peek().line,"variable name",peek().val, peek().file));
    do {
        if(peek().type == token::COMMA) consume();
        // ui_control
        if (peek().type == token::UI_CONTROL xor peek(1).type == token::UI_CONTROL) {
            auto parsed_ui_control = parse_declare_ui_control(node_declare_statement.get());
            if (parsed_ui_control.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_ui_control.get_error());
            }
            to_be_declared->params.push_back(std::move(parsed_ui_control.unwrap()));
        // array
        } else if (is_array_declaration()) {
            auto parsed_arr = parse_declare_array(node_declare_statement.get());
            if(parsed_arr.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_arr.get_error());
            }
            to_be_declared->params.push_back(std::move(parsed_arr.unwrap()));
        // variable
        } else if(is_variable_declaration()) {
            auto parsed_var = parse_declare_variable(node_declare_statement.get());
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
		auto assignee = parse_param_list(node_declare_statement.get());
		if(assignee.is_error()) {
			return Result<std::unique_ptr<NodeDeclareStatement>>(assignee.get_error());
		}
		assignees = std::move(assignee.unwrap());
	} else
	    // initializes empty param list
	    assignees = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));

    assignees->parent = node_declare_statement.get();
    node_declare_statement->to_be_declared = std::move(to_be_declared);
    node_declare_statement->assignee = std::move(assignees);
    node_declare_statement->parent = parent;
//	auto return_value = std::make_unique<NodeDeclareStatement>(std::move(to_be_declared), std::move(assignees), get_tok());
	return Result<std::unique_ptr<NodeDeclareStatement>>(std::move(node_declare_statement));
//    auto node_assign_statement = parse_into_assign_statement(std::move(to_be_declared), node_declare_statement.get());
//    if (node_assign_statement.is_error()) {
//        return Result<std::unique_ptr<NodeDeclareStatement>>(node_assign_statement.get_error());
//    }
//    node_declare_statement->statement = std::move(node_assign_statement.unwrap());
//    node_declare_statement->statement->parent = node_declare_statement.get();
//    node_declare_statement->parent = parent;
//	return Result<std::unique_ptr<NodeDeclareStatement>>(std::move(node_declare_statement));
}

Result<std::unique_ptr<NodeAssignStatement>> Parser::parse_into_assign_statement(std::unique_ptr<NodeParamList> array_variable, NodeAST* parent) {
    auto node_assign_statement = std::make_unique<NodeAssignStatement>(get_tok());
    std::unique_ptr<NodeParamList> assignees = nullptr;
    // if there is an assignment following
    if (peek().type == token::ASSIGN) {
        consume(); //consume :=
        auto assignee = parse_param_list(node_assign_statement.get());
        if(assignee.is_error()) {
            return Result<std::unique_ptr<NodeAssignStatement>>(assignee.get_error());
        }
        assignees = std::move(assignee.unwrap());
    } else
        // initializes empty param list
        assignees = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));

    assignees->parent = node_assign_statement.get();
    node_assign_statement->array_variable = std::move(array_variable);
    node_assign_statement->array_variable->parent = node_assign_statement.get();
    node_assign_statement->assignee = std::move(assignees);
    node_assign_statement->parent = parent;
    return Result<std::unique_ptr<NodeAssignStatement>>(std::move(node_assign_statement));
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


Result<std::unique_ptr<NodeVariable>> Parser::parse_declare_variable(NodeAST* parent) {
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
    auto parsed_var = parse_variable(parent, is_persistent, var_type);
    if(parsed_var.is_error()) {
        return Result<std::unique_ptr<NodeVariable>>(parsed_var.get_error());
    }
    return Result<std::unique_ptr<NodeVariable>>(std::move(parsed_var.unwrap()));
}

Result<std::unique_ptr<NodeArray>> Parser::parse_declare_array(NodeAST* parent) {
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
    auto parsed_arr = parse_array(parent, is_persistent, var_type);
    if(parsed_arr.is_error()) {
        return Result<std::unique_ptr<NodeArray>>(parsed_arr.get_error());
    }
    auto array = std::move(parsed_arr.unwrap());
    std::swap(array->indexes, array->sizes);
    return Result<std::unique_ptr<NodeArray>>(std::move(array));
}

Result<std::unique_ptr<NodeUIControl>> Parser::parse_declare_ui_control(NodeAST* parent) {
    auto node_ui_control = std::make_unique<NodeUIControl>(get_tok());
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
        auto parsed_arr = parse_array(node_ui_control.get(), is_persistent, var_type);
        if(parsed_arr.is_error()) {
            return Result<std::unique_ptr<NodeUIControl>>(parsed_arr.get_error());
        }
        auto array = std::move(parsed_arr.unwrap());
        std::swap(array->indexes, array->sizes);
        control_var = std::move(array);
    } else {
        auto parsed_var = parse_variable(node_ui_control.get(), is_persistent, var_type);
        if(parsed_var.is_error()) {
            return Result<std::unique_ptr<NodeUIControl>>(parsed_var.get_error());
        }
        control_var = std::move(parsed_var.unwrap());
    }
    std::unique_ptr<NodeParamList> control_params;
    if(peek().type == token::OPEN_PARENTH) {
        auto param_list = parse_param_list(node_ui_control.get());
        if (param_list.is_error()) {
            Result<std::unique_ptr<NodeUIControl>>(param_list.get_error());
        }
        control_params = std::move(param_list.unwrap());
    } else {
        control_params = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    }
    control_params->parent = node_ui_control.get();
    node_ui_control->ui_control_type = std::move(ui_control_type);
    node_ui_control->control_var = std::move(control_var);
    node_ui_control->params = std::move(control_params);
//    auto result = std::make_unique<NodeUIControl>(std::move(ui_control_type), std::move(control_var), std::move(control_params), get_tok());
    return Result<std::unique_ptr<NodeUIControl>>(std::move(node_ui_control));
}

Result<std::unique_ptr<NodeIfStatement>> Parser::parse_if_statement(NodeAST* parent) {
    auto node_if_statement = std::make_unique<NodeIfStatement>(get_tok());
    //consume if
    consume();
    auto condition_result = parse_expression(node_if_statement.get());
    if(condition_result.is_error()) {
        return Result<std::unique_ptr<NodeIfStatement>>(condition_result.get_error());
    }
    auto condition = std::move(condition_result.unwrap());
//    if(not(condition->type == ASTType::Boolean || condition->type == ASTType::Comparison)) {
//        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
//        "If Statement needs condition.", peek().line, "condition", "", peek().file));
//    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
         "Expected linebreak after if-condition.", peek().line, "linebreak", peek().val, peek().file));
    }
	consume(); // consume linebreak
	_skip_linebreaks();
    std::vector<std::unique_ptr<NodeStatement>> if_stmts = {};
    while (peek().type != token::END_IF && peek().type != token::ELSE) {
		if(peek().type == END_IF or peek().type == ELSE) break;
        auto stmt = parse_statement(node_if_statement.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            if_stmts.push_back(std::move(stmt.unwrap()));
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
            auto stmt = parse_if_statement(node_if_statement.get());
            if (stmt.is_error()) {
                return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
            }
            auto stmt_val = std::make_unique<NodeStatement>(std::move(stmt.unwrap()), get_tok());
            else_stmts.push_back(std::move(stmt_val));
        } else {
            while (peek().type != token::END_IF) {
                auto stmt = parse_statement(node_if_statement.get());
                if (stmt.is_error()) {
                    return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
                }
                if(stmt.unwrap()->statement)
                    else_stmts.push_back(std::move(stmt.unwrap()));
            }
        }
    }
    if(not(peek().type == token::END_IF || no_end_if)) {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
     "Missing end token.", peek().line, "end if",peek().val, peek().file));
    }
    if (!no_end_if) consume();
    node_if_statement->condition = std::move(condition);
    node_if_statement->statements = std::move(if_stmts);
    node_if_statement->else_statements = std::move(else_stmts);
    node_if_statement->parent = parent;
//    auto return_value = std::make_unique<NodeIfStatement>(std::move(condition), std::move(if_stmts), std::move(else_stmts), get_tok());
    return Result<std::unique_ptr<NodeIfStatement>>(std::move(node_if_statement));
}

Result<std::unique_ptr<NodeForStatement>> Parser::parse_for_statement(NodeAST* parent) {
    auto node_for_statement = std::make_unique<NodeForStatement>(get_tok());
    //consume for
    consume();
    auto assign_stmt = parse_assign_statement(node_for_statement.get());
    if(assign_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForStatement>>(assign_stmt.get_error());
    }
    auto iterator = std::move(assign_stmt.unwrap());
    if(not(peek().type == token::TO || peek().type == token::DOWNTO)) {
        return Result<std::unique_ptr<NodeForStatement>>(CompileError(ErrorType::SyntaxError,
        "Incorrect <for-loop> Syntax.", peek().line, "to/downto", peek().val, peek().file));
    }
    Token to = consume(); //consume to or downto
    auto expression_stmt = parse_binary_expr(node_for_statement.get());
    if(expression_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForStatement>>(expression_stmt.get_error());
    }
    auto iterator_end = std::move(expression_stmt.unwrap());
    std::unique_ptr<NodeAST> step;
    if(peek().type == STEP) {
        consume(); // consume step
        auto step_expression = parse_binary_expr(node_for_statement.get());
        if(step_expression.is_error()) {
            return Result<std::unique_ptr<NodeForStatement>>(step_expression.get_error());
        }
        step = std::move(step_expression.unwrap());
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeForStatement>>(CompileError(ErrorType::SyntaxError,
         "Missing linebreak in <for-loop>", peek().line, "linebreak", peek().val, peek().file));
    }
    consume(); //consume linebreak
    std::vector<std::unique_ptr<NodeStatement>> stmts = {};
    while (peek().type != token::END_FOR) {
        auto stmt = parse_statement(node_for_statement.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeForStatement>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            stmts.push_back(std::move(stmt.unwrap()));
    }
    consume(); // consume end for
    node_for_statement->iterator = std::move(iterator);
    node_for_statement->to = to;
    node_for_statement->iterator_end = std::move(iterator_end);
    node_for_statement->statements = std::move(stmts);
    node_for_statement->step = std::move(step);
    node_for_statement->parent = parent;
//    auto return_value = std::make_unique<NodeForStatement>(std::move(iterator), to, std::move(iterator_end), std::move(stmts), get_tok());
    return Result<std::unique_ptr<NodeForStatement>>(std::move(node_for_statement));
}

Result<std::unique_ptr<NodeWhileStatement>> Parser::parse_while_statement(NodeAST* parent) {
    auto node_while_statement = std::make_unique<NodeWhileStatement>(get_tok());
    consume(); // consume while
    auto condition_result = parse_expression(node_while_statement.get());
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
        auto stmt = parse_statement(node_while_statement.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeWhileStatement>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            stmts.push_back(std::move(stmt.unwrap()));
    }
    consume(); // consume end for
    node_while_statement->condition = std::move(condition);
    node_while_statement->statements = std::move(stmts);
    node_while_statement ->parent = parent;
//    auto return_value = std::make_unique<NodeWhileStatement>(std::move(condition), std::move(stmts), get_tok());
    return Result<std::unique_ptr<NodeWhileStatement>>(std::move(node_while_statement));
}

Result<std::unique_ptr<NodeSelectStatement>> Parser::parse_select_statement(NodeAST* parent) {
    auto node_select_statement = std::make_unique<NodeSelectStatement>(get_tok());
	consume(); //consume select
	auto expression = parse_expression(node_select_statement.get());
	if(peek().type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		"Expected linebreak after select-expression.", peek().line, "linebreak", peek().val, peek().file));
	}
	consume(); //consume linebreak
	_skip_linebreaks();
	if(peek().type != token::CASE) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		 "Expected cases in select-expression.", peek().line, "case <expression>", peek().val, peek().file));
	}
	std::map<std::vector<std::unique_ptr<NodeAST>>, std::vector<std::unique_ptr<NodeStatement>>> cases;
	while (peek().type != token::END_SELECT) {
		_skip_linebreaks();
		if(peek().type == token::CASE) {
			consume(); //consume case
            std::vector<std::unique_ptr<NodeAST>> cas = {};
			auto cas_result = parse_expression(node_select_statement.get());
			if(cas_result.is_error())
				return Result<std::unique_ptr<NodeSelectStatement>>(cas_result.get_error());
            cas.push_back(std::move(cas_result.unwrap()));
            if(peek().type == token::TO) {
                consume(); // consume to
                auto cas2_result = parse_expression(node_select_statement.get());
                if(cas2_result.is_error())
                    return Result<std::unique_ptr<NodeSelectStatement>>(cas2_result.get_error());
                cas.push_back(std::move(cas2_result.unwrap()));
            }
			if(peek().type != token::LINEBRK) {
				return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
				 "Expected linebreak after case.", peek().line, "linebreak", peek().val, peek().file));
			}
			consume(); //consume linebreak
			std::vector<std::unique_ptr<NodeStatement>> stmts = {};
			while(peek().type != token::END_SELECT && peek().type != token::CASE) {
				auto stmt = parse_statement(node_select_statement.get());
				if (stmt.is_error()) {
					return Result<std::unique_ptr<NodeSelectStatement>>(stmt.get_error());
				}
                if(stmt.unwrap()->statement)
                    stmts.push_back(std::move(stmt.unwrap()));
			}
			cases.insert(std::make_pair(std::move(cas),std::move( stmts)));
		}
	}
	consume(); // consume end select
    node_select_statement->expression = std::move(expression.unwrap());
    node_select_statement->cases = std::move(cases);
    node_select_statement->parent = parent;
//	auto return_value = std::make_unique<NodeSelectStatement>(std::move(expression.unwrap()), std::move(cases), get_tok());
	return Result<std::unique_ptr<NodeSelectStatement>>(std::move(node_select_statement));
}

//Result<std::unique_ptr<NodeDefineStatement>> Parser::parse_define_statement() {
//	consume(); //consume define keyword
//	VarType type = VarType::Define;
//	auto definitions = parse_param_list();
//	if(definitions.is_error()) {
//		return Result<std::unique_ptr<NodeDefineStatement>>(definitions.get_error());
//	}
//	if(peek().type != token::ASSIGN) {
//		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
//		 "Defines need to have expressions assigned.", peek().m_line, ":=", peek().val, peek().file));
//	}
//	consume(); // consume assign :=
//	auto assignees = parse_param_list();
//	if(assignees.is_error()) {
//		return Result<std::unique_ptr<NodeDefineStatement>>(assignees.get_error());
//	}
//	auto return_value = std::make_unique<NodeDefineStatement>(std::move(definitions.unwrap()), std::move(assignees.unwrap()), get_tok());
//	return Result<std::unique_ptr<NodeDefineStatement>>(std::move(return_value));
//}

Result<std::unique_ptr<NodeAST>> Parser::parse_family_statement(NodeAST* parent) {
	auto node_family_statement = std::make_unique<NodeFamilyStatement>(get_tok());
	Token construct = consume(); //consume family
	token end_construct = token::END_FAMILY;
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
		 "Found unknown family syntax.", peek().line, "valid prefix", peek().val, peek().file));
	}
	auto prefix = consume(); //consume prefix
	auto l = consume_linebreak("<family statement>");
	if(l.is_error())
		return Result<std::unique_ptr<NodeAST>>(l.get_error());
	std::vector<std::unique_ptr<NodeStatement>> stmts;
	while(peek().type != token::END_FAMILY) {
		_skip_linebreaks();
		auto declare_stmt = parse_statement(nullptr);
		if(declare_stmt.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(declare_stmt.get_error());
		}
		declare_stmt.unwrap() -> parent = node_family_statement.get();
        if(declare_stmt.unwrap()->statement)
		    stmts.push_back(std::move(declare_stmt.unwrap()));
	}
	consume(); // consume end family
	node_family_statement->prefix = prefix.val;
	node_family_statement->members = std::move(stmts);
	node_family_statement -> parent = parent;
	return Result<std::unique_ptr<NodeAST>>(std::move(node_family_statement));
}


Result<std::unique_ptr<NodeAST>> Parser::parse_const_struct_family_statement(NodeAST* parent) {
    Token construct = consume(); //consume family, struct, const
    token end_construct = token::END_CONST;
    if (construct.type == token::FAMILY) {
        end_construct = token::END_FAMILY;
    } else if (construct.type == token::STRUCT) {
        end_construct = token::END_STRUCT;
    }

    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
            "Found unknown const/family/struct syntax.", peek().line, "valid prefix", peek().val, peek().file));
    }
    auto prefix = consume(); //consume prefix
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
            "Expected linebreak.", peek().line, "linebreak", peek().val, peek().file));
    }
    consume(); // consume linebreak

    std::vector<std::unique_ptr<NodeStatement>> stmts;
    while(peek().type != end_construct) {
        _skip_linebreaks();
        auto declare_stmt = parse_declare_statement(nullptr);
        if(declare_stmt.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(declare_stmt.get_error());
        }
		auto node_stmt = std::make_unique<NodeStatement>(std::move(declare_stmt.unwrap()), get_tok());
		node_stmt->statement->parent = node_stmt.get();
        stmts.push_back(std::move(node_stmt));

		auto l = consume_linebreak("<statement>");
		if(l.is_error())
			return Result<std::unique_ptr<NodeAST>>(l.get_error());
    }
    consume();

	std::unique_ptr<NodeAST> return_value = nullptr;
	if (construct.type == token::CONST) {
		auto node_const_statement = std::make_unique<NodeConstStatement>(std::move(prefix.val), std::move(stmts), get_tok());
		for (auto &stmt : node_const_statement->constants) { stmt->parent = node_const_statement.get(); }
		return_value = std::move(node_const_statement);
	} else if (construct.type == token::STRUCT) {
		auto node_struct_statement = std::make_unique<NodeStructStatement>(std::move(prefix.val), std::move(stmts), get_tok());
		for (auto &stmt : node_struct_statement->members) { stmt->parent = node_struct_statement.get(); }
		return_value = std::move(node_struct_statement);
	}
    return_value -> parent = parent;
	// set the parent for each statement in stmts
    return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
}

Result<SuccessTag> Parser::consume_linebreak(const std::string& construct) {
	if(peek().type != token::LINEBRK) {
		return Result<SuccessTag>(CompileError(ErrorType::SyntaxError,
		 "Missing linebreak in "+construct+".", peek().line, "linebreak", peek().val, peek().file));
	}
	consume(); // consume linebreak
	return Result<SuccessTag>(SuccessTag{});
}


Result<std::unique_ptr<NodeGetControlStatement>> Parser::parse_get_control_statement(std::unique_ptr<NodeAST> ui_id, NodeAST* parent) {
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
    auto node_get_control_statement = std::make_unique<NodeGetControlStatement>(std::move(ui_id), control_param, get_tok());
	node_get_control_statement->ui_id->parent = node_get_control_statement.get();
	node_get_control_statement->parent = parent;
    return Result<std::unique_ptr<NodeGetControlStatement>>(std::move(node_get_control_statement));
}






