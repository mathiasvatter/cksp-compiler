//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"
#include "../AST/ASTVisitor/ASTVisitor.h"

#include <filesystem>
#include <utility>


Parser::Parser(std::vector<Token> tokens): Processor(std::move(tokens)) {}

Result<std::unique_ptr<NodeProgram>> Parser::parse() {
	return parse_program();
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
            CompileError(ErrorType::ParseError, "Invalid integer format.", expected, tok));
    }
}


Result<std::unique_ptr<NodeString>> Parser::parse_string(NodeAST* parent) {
	auto string = consume();
    auto node_string = std::make_unique<NodeString>(std::move(string.val), string);
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
			CompileError(ErrorType::ParseError, "Invalid real format.", "valid real", value));
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
    CompileError(ErrorType::ParseError, "Unknown number type.",  "INT, REAL, HEXADECIMAL, BINARY", value));
}

Result<std::unique_ptr<NodeVariable>> Parser::parse_variable(NodeAST* parent, const std::optional<Token>& is_persistent, DataType var_type) {
    auto var_token = consume();
    std::string var_name = var_token.val;
	auto type = infer_type_from_identifier(var_name);
    auto node_variable = std::make_unique<NodeVariable>(is_persistent, var_name, var_type, var_token);
    node_variable->parent = parent;
	node_variable->type = type;
    return Result<std::unique_ptr<NodeVariable>>(std::move(node_variable));
}

Result<std::unique_ptr<NodeVariableRef>> Parser::parse_variable_ref(NodeAST* parent) {
	auto var_token = consume();
	std::string var_name = var_token.val;
	auto type = infer_type_from_identifier(var_name);
	auto node_variable_ref = std::make_unique<NodeVariableRef>(var_name, var_token);
	node_variable_ref->parent = parent;
	node_variable_ref->type = type;
	return Result<std::unique_ptr<NodeVariableRef>>(std::move(node_variable_ref));
}

Result<std::unique_ptr<NodeDataStructure>> Parser::parse_array(NodeAST *parent, bool is_reference, std::optional<Token> is_persistent, DataType var_type) {
	auto error = CompileError(ErrorType::SyntaxError,"Found unknown Array Syntax.", "", peek());
    auto arr_token = consume();
    std::string arr_name = arr_token.val;
	auto type = infer_type_from_identifier(arr_name);
	std::unique_ptr<NodeDataStructure> node_array = nullptr;
    std::unique_ptr<NodeParamList> indexes = std::unique_ptr<NodeParamList>(new NodeParamList({}, arr_token));;
    indexes->parent = node_array.get();
	std::unique_ptr<NodeParamList> sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, arr_token));
    sizes->parent = node_array.get();
    if(peek().type == token::OPEN_BRACKET) {
        consume(); // consume [
        // if it is an empty array initialization
        if (peek().type != token::CLOSED_BRACKET) {
            auto index_params = parse_param_list(node_array.get());
            if (index_params.is_error()) {
                return Result<std::unique_ptr<NodeDataStructure>>(index_params.get_error());
            }
            indexes = std::move(index_params.unwrap());
        }
        if(peek().type != token::CLOSED_BRACKET) {
			error.m_expected = "]";
			return Result<std::unique_ptr<NodeDataStructure>>(error);
		}
        consume(); // consume ]
    } else {
		error.m_expected = "[";
        return Result<std::unique_ptr<NodeDataStructure>>(error);
    }
	if(!is_reference) std::swap(sizes,indexes);

	// if it is multidimensional array
	if(indexes->params.size() > 1 || sizes->params.size() > 1) {
		auto node = std::make_unique<NodeNDArray>(arr_name, arr_token);
		node->dimensions = std::max(indexes->params.size(), sizes->params.size());
		node->sizes = std::move(sizes);
		node->indexes = std::move(indexes);
		node_array = std::move(node);
	} else {
		auto node = std::make_unique<NodeArray>(arr_name, arr_token);
		if(!sizes->params.empty()) node->size = std::move(sizes->params[0]);
		if(!indexes->params.empty()) node->index = std::move(indexes->params[0]);
		node_array = std::move(node);
		node_array->set_child_parents();
	}

	node_array->is_reference = is_reference;
	node_array->parent = parent;
	node_array->persistence = std::move(is_persistent);
	node_array->is_local = false;
	node_array->data_type = var_type;
	node_array->type = type;
	return Result<std::unique_ptr<NodeDataStructure>>(std::move(node_array));
}

Result<std::unique_ptr<NodeReference>> Parser::parse_array_ref(NodeAST *parent) {
	auto error = CompileError(ErrorType::SyntaxError,"Found unknown Array Reference Syntax.", "", peek());
	auto arr_token = consume();
	std::string arr_name = arr_token.val;
	auto type = infer_type_from_identifier(arr_name);
	std::unique_ptr<NodeReference> node_array_ref = nullptr;
	std::unique_ptr<NodeParamList> indexes = std::unique_ptr<NodeParamList>(new NodeParamList({}, arr_token));;
	indexes->parent = node_array_ref.get();
	if(peek().type == token::OPEN_BRACKET) {
		consume(); // consume [
		// if it is an empty array initialization
		if (peek().type == token::CLOSED_BRACKET) {
			error.m_message += "Empty Array Reference Initialization is not allowed.";
			return Result<std::unique_ptr<NodeReference>>(error);
		}
		auto index_params = parse_param_list(node_array_ref.get());
		if (index_params.is_error()) {
			return Result<std::unique_ptr<NodeReference>>(index_params.get_error());
		}
		indexes = std::move(index_params.unwrap());
		if(peek().type != token::CLOSED_BRACKET) {
			error.m_expected = "]";
			return Result<std::unique_ptr<NodeReference>>(error);
		}
		consume(); // consume ]
	} else {
		error.m_expected = "[";
		return Result<std::unique_ptr<NodeReference>>(error);
	}
	// if it is multidimensional array reference
	if(indexes->params.size() > 1) {
		auto node = std::make_unique<NodeNDArrayRef>(arr_name, std::move(indexes), arr_token);
		node_array_ref = std::move(node);
	} else {
		auto node = std::make_unique<NodeArrayRef>(arr_name, std::move(indexes->params[0]),arr_token);
		node_array_ref = std::move(node);
	}
	node_array_ref->parent = parent;
	node_array_ref->type = type;
	return Result<std::unique_ptr<NodeReference>>(std::move(node_array_ref));
}


Result<std::unique_ptr<NodeAST>> Parser::parse_expression(NodeAST* parent) {
    auto lhs = parse_string_expr(parent);
    if(lhs.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(lhs.get_error());
    }
    return _parse_string_expr_rhs(std::move(lhs.unwrap()), parent);
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
        if(peek().type != token::STRING_OP) {
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
		auto node_binary_expr = std::make_unique<NodeBinaryExpr>(string_op.type, std::move(lhs), std::move(rhs.unwrap()), get_tok());
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
	if(peek().type == token::RETURN) {
		m_tokens[m_pos].type = token::KEYWORD;
	}
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
            auto var_array = parse_array(parent, true);
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
    } else if (contains(UNARY_TOKENS, peek().type)){
        return parse_unary_expr(parent);
    } else {
        return Result<std::unique_ptr<NodeAST>>(
    CompileError(ErrorType::ParseError,"Found unknown expression token.", "keyword, integer, parenthesis", peek()));
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_unary_expr(NodeAST* parent) {
    auto node_unary_expr = std::make_unique<NodeUnaryExpr>(get_tok());
    Token unary_op = consume();
    auto expr = _parse_primary_expr(node_unary_expr.get());
    if(expr.is_error()) {
        return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    }
    node_unary_expr->op = unary_op.type;
    node_unary_expr->operand = std::move(expr.unwrap());
    node_unary_expr->set_child_parents();
    node_unary_expr->parent = parent;
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
		if (contains(COMPARISON_TOKENS, bin_op.type)) {
            // Check if rhs is NodeComparisonExpr because comparisons in comparisons are not allowed
            if (lhs->type == ASTType::Comparison) {
                return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
                 "Nested Comparisons are not allowed.", "valid expression operator", bin_op));
            }
            type = ASTType::Comparison;
		} else if (bin_op.type == token::BOOL_AND || bin_op.type == token::BOOL_OR){
			type = ASTType::Boolean;
		}

		auto node_binary_expr = std::make_unique<NodeBinaryExpr>(bin_op.type, std::move(lhs), std::move(rhs.unwrap()), get_tok());
        node_binary_expr->parent = parent;
        lhs = std::move(node_binary_expr);
        lhs->type = type;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::_parse_parenth_expr(NodeAST* parent) {
    consume(); // eat (
    auto expr = parse_binary_expr(parent);
    if (peek().type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::ParseError,
		 "Missing parenthesis.",  ")", peek()));
    }
    consume(); // eat )
    return expr;
}

Result<std::unique_ptr<NodeSingleAssignStatement>> Parser::parse_single_assign_statement(NodeAST* parent) {
    auto node_assign_statement_res = parse_assign_statement(parent);
    if(node_assign_statement_res.is_error())
        Result<std::unique_ptr<NodeSingleAssignStatement>>(node_assign_statement_res.get_error());
    auto node_assign_statement = std::move(node_assign_statement_res.unwrap());
    if(node_assign_statement->array_variable->params.size() != 1) {
        return Result<std::unique_ptr<NodeSingleAssignStatement>>(CompileError(ErrorType::ParseError,
         "Incorrect Syntax in <Single Assign Statement>.", peek().line, "One Assignment", std::to_string(node_assign_statement->array_variable->params.size()), peek().file));
    }
    if(node_assign_statement->assignee->params.size() != 1) {
        return Result<std::unique_ptr<NodeSingleAssignStatement>>(CompileError(ErrorType::ParseError,
        "Incorrect Syntax in <Single Assign Statement>.", peek().line, "One Assignment", std::to_string(node_assign_statement->assignee->params.size()), peek().file));
    }
    auto node_single_assign_statement = std::make_unique<NodeSingleAssignStatement>(
            std::move(node_assign_statement->array_variable->params[0]), std::move(node_assign_statement->assignee->params[0]), get_tok());
    return Result<std::unique_ptr<NodeSingleAssignStatement>>(std::move(node_single_assign_statement));
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
"Found invalid Assign Statement Syntax.", ":=", peek()));
    }
    consume(); // consume :=
    auto assignee =  parse_param_list(node_assign_statement.get()); //_parse_assignee();
    if(assignee.is_error()) {
        return Result<std::unique_ptr<NodeAssignStatement>>(assignee.get_error());
    }
    node_assign_statement->array_variable = std::move(vars);
    node_assign_statement->assignee = std::move(assignee.unwrap());
    node_assign_statement->set_child_parents();
    node_assign_statement->parent = parent;
    return Result<std::unique_ptr<NodeAssignStatement>>(std::move(node_assign_statement));
}

Result<std::unique_ptr<NodeReturnStatement>> Parser::parse_return_statement(NodeAST* parent) {
	consume(); // consume return keyword
	auto node_return_statement = std::make_unique<NodeReturnStatement>(get_tok());
	while(peek().type != token::LINEBRK) {
		auto expr_result = parse_expression(node_return_statement.get());
		if(expr_result.is_error()) {
			return Result<std::unique_ptr<NodeReturnStatement>>(expr_result.get_error());
		}
		node_return_statement->return_variables.push_back(std::move(expr_result.unwrap()));
		if(peek().type == token::COMMA) consume();
	}
    node_return_statement->set_child_parents();
	node_return_statement->parent = parent;
	return Result<std::unique_ptr<NodeReturnStatement>>(std::move(node_return_statement));
}

Result<std::unique_ptr<NodeStatement>> Parser::parse_statement(NodeAST* parent) {
    auto node_statement = std::make_unique<NodeStatement>(get_tok());
    _skip_linebreaks();
    std::unique_ptr<NodeAST> stmt;
    // assign statement
    if (peek().type == token::KEYWORD || peek().type == token::RETURN || peek().type == token::DECLARE
		|| peek().type == token::CALL || peek().type == token::SET_CONDITION || peek().type == token::RESET_CONDITION) {
        if (peek().type == token::DECLARE) {
            auto declare_stmt = parse_declare_statement(node_statement.get());
            if (declare_stmt.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(declare_stmt.get_error());
            }
            stmt = std::move(declare_stmt.unwrap());
        } else if ((peek().type == token::CALL) xor
                   (peek(1).type == token::OPEN_PARENTH or peek(1).type == token::LINEBRK or peek(1).type == token::CLOSED_PARENTH)) {
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
    } else if (peek().type == token::CONST) {
		auto construct_stmt = parse_const_statement(node_statement.get());
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
        if(is_ranged_for_loop()) {
            auto for_stmt = parse_for_each_statement(node_statement.get());
            if (for_stmt.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(for_stmt.get_error());
            }
            stmt = std::move(for_stmt.unwrap());
        } else {
            auto for_stmt = parse_for_statement(node_statement.get());
            if (for_stmt.is_error()) {
                return Result<std::unique_ptr<NodeStatement>>(for_stmt.get_error());
            }
            stmt = std::move(for_stmt.unwrap());
        }
    } else if (peek().type == token::WHILE) {
        auto while_stmt = parse_while_statement(node_statement.get());
        if(while_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(while_stmt.get_error());
        }
        stmt = std::move(while_stmt.unwrap());
	} else if (peek().type == token::SELECT) {
        auto select_stmt = parse_select_statement(node_statement.get());
        if (select_stmt.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(select_stmt.get_error());
        }
        stmt = std::move(select_stmt.unwrap());
    } else if (peek().type == token::LIST) {
        auto list_block = parse_list_block(node_statement.get());
        if (list_block.is_error()) {
            return Result<std::unique_ptr<NodeStatement>>(list_block.get_error());
        }
        stmt = std::move(list_block.unwrap());
    } else {
        return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
         "Found invalid Statement Syntax.", "Statement",peek()));
    }
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeStatement>>(CompileError(ErrorType::SyntaxError,
		"Found incorrect statement syntax.", "", peek()));
    }
    consume();
    _skip_linebreaks();
    node_statement->statement = std::move(stmt);
    node_statement->parent = parent;
    return Result<std::unique_ptr<NodeStatement>>(std::move(node_statement));
}


Result<std::unique_ptr<NodeCallback>> Parser::parse_callback(NodeAST* parent) {
    auto node_callback = std::make_unique<NodeCallback>(get_tok());
    auto node_body = std::make_unique<NodeBody>(get_tok());
    std::string begin_callback = consume().val;
    std::unique_ptr<NodeAST> callback_id;
    if(begin_callback == "on ui_control") {
        if (peek().type != token::OPEN_PARENTH) {
            return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
             "Missing open parenthesis after <callback>",  "(", peek()));
        }
        consume(); // consume (
        auto callback_id_result = parse_binary_expr(node_callback.get());
        if (callback_id_result.is_error()) {
            Result<std::unique_ptr<NodeFunctionHeader>>(callback_id_result.get_error());
        }
        callback_id = std::move(callback_id_result.unwrap());
        if (peek().type != token::CLOSED_PARENTH) {
            return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
              "Missing closing parenthesis after <callback>",  ")", peek()));
        }
        consume(); // consume )
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
      "Missing linebreak after <callback>",  "linebreak",peek()));
    }
    consume(); // Consume LINEBREAK
    _skip_linebreaks();
    std::vector<std::unique_ptr<NodeStatement>> stmts;
    while (peek().type != token::END_CALLBACK) {
        if (peek().type == token::BEGIN_CALLBACK) {
            return Result<std::unique_ptr<NodeCallback>>(CompileError(ErrorType::ParseError,
         "", "end on", peek()));
        }
        auto stmt = parse_statement(node_body.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeCallback>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            stmts.push_back(std::move(stmt.unwrap()));
    }
    node_body->statements = std::move(stmts);
    std::string end_callback = consume().val; // Consume END_CALLBACK
    node_callback->begin_callback = std::move(begin_callback);
    node_callback->callback_id = std::move(callback_id);
    node_callback->statements = std::move(node_body);
    node_callback->end_callback = std::move(end_callback);
    node_callback->set_child_parents();
    node_callback->parent = parent;
    return Result<std::unique_ptr<NodeCallback>>(std::move(node_callback));
}

Result<std::unique_ptr<NodeProgram>> Parser::parse_program() {
    std::vector<std::unique_ptr<NodeCallback>> callbacks;
    auto node_program = std::make_unique<NodeProgram>(get_tok());
    while (peek().type != token::END_TOKEN) {
        _skip_linebreaks();
        if (peek().type == token::BEGIN_CALLBACK) {
            auto callback = parse_callback(node_program.get());
            if (callback.is_error())
                return Result<std::unique_ptr<NodeProgram>>(callback.get_error());
			callbacks.push_back(std::move(callback.unwrap()));
        } else if (peek().type == token::FUNCTION) {
            auto function = parse_function_definition(node_program.get());
            if (function.is_error())
                return Result<std::unique_ptr<NodeProgram>>(function.get_error());
            auto node_function = std::move(function.unwrap());
            auto hash_value = StringIntKey{node_function->header->name, (int)node_function->header->args->params.size()};
            auto it = m_function_definitions.find({node_function->header->name, (int)node_function->header->args->params.size()});
            // if function already defined
            if(it != m_function_definitions.end()) {
                if(node_function->override) {
                    m_function_definitions[hash_value] = std::move(node_function);
                } else if(!it->second->override){
                    return Result<std::unique_ptr<NodeProgram>>(CompileError(ErrorType::SyntaxError,
                        "Function has already been defined.","",node_function->header->tok));
                }
            } else {
                m_function_definitions[hash_value] = std::move(node_function);
            }

        } else {
            return Result<std::unique_ptr<NodeProgram>>(CompileError(ErrorType::ParseError,
             "Found unknown construct.", "<callback>, <function_definition>", peek()));
        }
        auto l = consume_linebreak("<program-level construct>");
        if(l.is_error())
            return Result<std::unique_ptr<NodeProgram>>(l.get_error());
        _skip_linebreaks();
    }
    node_program->callbacks = std::move(callbacks);
    for(auto & func_def : m_function_definitions) {
        node_program->function_definitions.push_back(std::move(func_def.second));
    }
    return Result<std::unique_ptr<NodeProgram>>(std::move(node_program));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_param_list(NodeAST* parent) {
    auto param_list = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
    param_list->parent = parent;
    auto result = _parse_into_param_list(param_list->params, param_list.get());
    if (result.is_error()) {
        return Result<std::unique_ptr<NodeParamList>>(result.get_error());
    }
    return Result<std::unique_ptr<NodeParamList>>(std::move(param_list));
}

Result<SuccessTag> Parser::_parse_into_param_list(std::vector<std::unique_ptr<NodeAST>>& params, NodeAST* parent) {
    size_t end_backup_pos; // in case of declaration list and ui_slider sliddd(0,1), <-
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
            if (!exprResult.is_error()) {
                params.push_back(std::move(exprResult.unwrap()));
            } else {
                // eg case declare ui_slider sli_bum(0,100), ui_button btn_buuuuu
                if(peek(end_backup_pos-m_pos).type == token::COMMA) {
                    m_pos = end_backup_pos;
                    break;
                }
                return Result<SuccessTag>(exprResult.get_error());
            }
        }
        if (peek().type != token::COMMA) break;
        end_backup_pos = m_pos;
        consume(); // consume comma
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header(NodeAST* parent) {
    auto node_function_header = std::make_unique<NodeFunctionHeader>(get_tok());
    std::string func_name;
    std::unique_ptr<NodeParamList> func_args = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
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
    node_function_header->set_child_parents();
    node_function_header->parent = parent;
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
    node_function_call->set_child_parents();
    node_function_call->parent = parent;
    mark_function_as_used(node_function_call->function->name, node_function_call->function->args->params.size());
    return Result<std::unique_ptr<NodeFunctionCall>>(std::move(node_function_call));
}


Result<std::unique_ptr<NodeFunctionDefinition>> Parser::parse_function_definition(NodeAST* parent) {
    consume(); //consume "function"
    auto node_function_definition = std::make_unique<NodeFunctionDefinition>(get_tok());
    std::unique_ptr<NodeFunctionHeader> func_header;
    std::optional<std::unique_ptr<NodeParamList>> func_return_var;
    auto func_body = std::make_unique<NodeBody>(get_tok());
    bool func_override = false;
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
        "Missing function name.","keyword",peek()));
    }
    auto header = parse_function_header(node_function_definition.get());
    if (header.is_error()) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(header.get_error());
    }
    func_header = std::move(header.unwrap());
    func_return_var = {};
    if (peek().type == token::ARROW) {
        consume();
		if(peek().type == token::RETURN) {
			m_tokens[m_pos].type = token::KEYWORD;
		}
        if (peek().type == token::KEYWORD) {
            auto return_var = parse_param_list(node_function_definition.get());
            if (return_var.is_error()) {
                Result<std::unique_ptr<NodeFunctionDefinition>>(return_var.get_error());
            }
            func_return_var = std::move(return_var.unwrap());
        } else {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::ParseError,
             "Missing return variable after ->","return variable",peek()));
        }
    }
    if (peek().type == token::OVERRIDE) {
        consume();
        func_override = true;
    }

    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeFunctionDefinition>>(CompileError(ErrorType::SyntaxError,
        "Missing necessary linebreak after function header.","linebreak",peek()));
    }
    consume(); // consume linebreak

    while (peek().type != token::END_FUNCTION) {
        _skip_linebreaks();
        if(peek().type == token::END_FUNCTION) break;
        auto stmt = parse_statement(node_function_definition.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeFunctionDefinition>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            func_body->statements.push_back(std::move(stmt.unwrap()));
    }
    consume();
    node_function_definition->header = std::move(func_header);
    node_function_definition->return_variable = std::move(func_return_var);
    node_function_definition->override = func_override;
    node_function_definition->body = std::move(func_body);
    node_function_definition->set_child_parents();
    node_function_definition->parent = parent;
    return Result<std::unique_ptr<NodeFunctionDefinition>>(std::move(node_function_definition));
}

Result<std::unique_ptr<NodeDeclareStatement>> Parser::parse_declare_statement(NodeAST* parent) {
    auto node_declare_statement = std::make_unique<NodeDeclareStatement>(get_tok());
    if(peek().type == token::DECLARE) consume(); //consume declare
    std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared;
    if(not(peek().type == token::KEYWORD or peek().type == token::UI_CONTROL or get_persistent_keyword(peek())
		or peek().type == token::CONST or peek().type == token::POLYPHONIC or peek().type== token::LOCAL or peek().type== token::GLOBAL))
        return Result<std::unique_ptr<NodeDeclareStatement>>(CompileError(ErrorType::ParseError,
        "Incorrect syntax in declare statement.","<ui_control>, <variable>, <array>",peek()));
    do {
        if(peek().type == token::COMMA) consume();
        // ui_control
        if (peek().type == token::UI_CONTROL xor peek(1).type == token::UI_CONTROL) {
            auto parsed_ui_control = parse_declare_ui_control(node_declare_statement.get());
            if (parsed_ui_control.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_ui_control.get_error());
            }
            to_be_declared.push_back(std::move(parsed_ui_control.unwrap()));
        // array
        } else if (is_array_declaration()) {
            auto parsed_arr = parse_declare_array(node_declare_statement.get());
            if(parsed_arr.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_arr.get_error());
            }
            to_be_declared.push_back(std::move(parsed_arr.unwrap()));
        // variable
        } else if(is_variable_declaration()) {
            auto parsed_var = parse_declare_variable(node_declare_statement.get());
            if (parsed_var.is_error()) {
                return Result<std::unique_ptr<NodeDeclareStatement>>(parsed_var.get_error());
            }
            to_be_declared.push_back(std::move(parsed_var.unwrap()));
        } else {
            return Result<std::unique_ptr<NodeDeclareStatement>>(CompileError(ErrorType::ParseError,
        "Incorrect syntax in declare statement.","<ui_control>, <variable>, <array>",peek()));
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

    node_declare_statement->to_be_declared = std::move(to_be_declared);
    node_declare_statement->assignee = std::move(assignees);
    node_declare_statement->set_child_parents();
    node_declare_statement->parent = parent;
	return Result<std::unique_ptr<NodeDeclareStatement>>(std::move(node_declare_statement));
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

    node_assign_statement->array_variable = std::move(array_variable);
    node_assign_statement->assignee = std::move(assignees);
    node_assign_statement->set_child_parents();
    node_assign_statement->parent = parent;
    return Result<std::unique_ptr<NodeAssignStatement>>(std::move(node_assign_statement));
}


bool Parser::is_variable_declaration() {
    // read local (const | polyphonic) keyword
    bool first = get_persistent_keyword(peek()) and (peek(1).type == token::LOCAL or peek(1).type == token::GLOBAL) and (peek(2).type == token::CONST || peek(2).type == token::POLYPHONIC) and peek(3).type == token::KEYWORD;
    // read (const | polyphonic) keyword
    bool second = get_persistent_keyword(peek()) and (peek(1).type == token::CONST || peek(1).type == token::POLYPHONIC) and peek(2).type == token::KEYWORD;
    // read keyword
    bool third = get_persistent_keyword(peek()) and peek(1).type == token::KEYWORD;
    // read local keyword
    bool sixth = get_persistent_keyword(peek()) and (peek(1).type == token::LOCAL or peek(1).type == token::GLOBAL) and peek(2).type == token::KEYWORD;
    // (const | polyphonic) keyword
    bool fifth = (peek().type == token::CONST || peek().type == token::POLYPHONIC) and peek(1).type == token::KEYWORD;
    // local keyword
    bool seventh = (peek().type == token::LOCAL or peek().type == token::GLOBAL) and peek(1).type == token::KEYWORD;
	// local (const | polyphonic) keyword
	bool eighth = (peek().type == token::LOCAL or peek().type == token::GLOBAL) and (peek(1).type == token::CONST || peek(1).type == token::POLYPHONIC) and peek(2).type == token::KEYWORD;
    // keyword
    bool fourth = peek().type == token::KEYWORD;

    return first xor second xor third xor fourth xor fifth xor sixth xor seventh xor eighth;
}

bool Parser::is_array_declaration() {
    // read local a[]
    bool first = get_persistent_keyword(peek()) and (peek(1).type == token::LOCAL or peek(1).type == token::GLOBAL) and peek(2).type == token::KEYWORD and peek(3).type == token::OPEN_BRACKET;
    // local a[]
    bool second = (peek().type == token::LOCAL or peek().type == token::GLOBAL) and peek(1).type == token::KEYWORD and peek(2).type == token::OPEN_BRACKET;
    // read a[]
    bool third = get_persistent_keyword(peek()) and peek(1).type == token::KEYWORD and peek(2).type == token::OPEN_BRACKET;
    // a[]
    bool fourth = peek().type == token::KEYWORD and peek(1).type == token::OPEN_BRACKET;

    return first xor second xor third xor fourth;
}

std::optional<Token> Parser::get_persistent_keyword(const Token& tok) {
    if(tok.type == token::READ || tok.type == token::PERS || tok.type == token::INSTPERS) {
        return tok;
    }
    return {};
}


Result<std::unique_ptr<NodeVariable>> Parser::parse_declare_variable(NodeAST* parent) {
    auto persistence = get_persistent_keyword(peek());
    if(persistence) {
        consume();
    }
    bool is_local = false;
	bool is_global = false;
    if(peek().type == token::LOCAL or peek().type == token::GLOBAL) {
        is_local = peek().type == token::LOCAL;
		is_global = peek().type == token::GLOBAL;
        consume();
    }
    DataType var_type = DataType::Mutable;
    if(peek().type == token::CONST || peek().type == token::POLYPHONIC) {
        if(peek().type == token::CONST)
            var_type = DataType::Const;
        else if (peek().type == token::POLYPHONIC)
            var_type = DataType::Polyphonic;
        consume();
    }
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeVariable>>(CompileError(ErrorType::SyntaxError,
            "Found unknown variable declaration syntax.", "variable keyword", peek()));
    }
    auto parsed_var = parse_variable(parent, persistence, var_type);
    if(parsed_var.is_error()) {
        return Result<std::unique_ptr<NodeVariable>>(parsed_var.get_error());
    }
	auto node_variable = std::move(parsed_var.unwrap());
	node_variable->is_local = is_local;
	node_variable->is_global = is_global;
    node_variable->is_reference = false;
    return Result<std::unique_ptr<NodeVariable>>(std::move(node_variable));
}

Result<std::unique_ptr<NodeDataStructure>> Parser::parse_declare_array(NodeAST* parent) {
    auto persistence = get_persistent_keyword(peek());
    if(persistence) {
        consume();
    }
    bool is_local = false;
	bool is_global = false;
    if(peek().type == token::LOCAL or peek().type == token::GLOBAL) {
        is_local = peek().type == token::LOCAL;
		is_global = peek().type == token::GLOBAL;
        consume();
    }
    DataType var_type = DataType::Array;
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeDataStructure>>(CompileError(ErrorType::SyntaxError,
																	   "Found unknown array declaration syntax.", "array keyword", peek()));
    }
    auto parsed_arr = parse_array(parent, false, persistence, var_type);
    if(parsed_arr.is_error()) {
        return Result<std::unique_ptr<NodeDataStructure>>(parsed_arr.get_error());
    }
    auto node_array = std::move(parsed_arr.unwrap());
	node_array->is_local = is_local;
	node_array->is_global = is_global;
    node_array->is_reference = false;
    return Result<std::unique_ptr<NodeDataStructure>>(std::move(node_array));
}

Result<std::unique_ptr<NodeUIControl>> Parser::parse_declare_ui_control(NodeAST* parent) {
    auto persistence = get_persistent_keyword(peek());
    if(persistence) {
        consume();
    }
    auto node_ui_control = std::make_unique<NodeUIControl>(get_tok());
    DataType var_type = DataType::UI_Control;
    if(peek().type != token::UI_CONTROL) {
        return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::SyntaxError,
   "Found unknown ui_control declaration syntax.", "valid ui_control type", peek()));
    }
    std::string ui_control_type = consume().val;
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::SyntaxError,
   "Found unknown ui_control declaration syntax.", "array or variable keyword", peek()));
    }
    std::unique_ptr<NodeDataStructure> control_var;
    // check if it has second Brackets -> ui_control array
    std::unique_ptr<NodeParamList> control_array_sizes;
    if(peek(1).type == token::OPEN_BRACKET) {
        auto parsed_arr = parse_array(node_ui_control.get(), false, persistence, var_type);
        if(parsed_arr.is_error()) {
            return Result<std::unique_ptr<NodeUIControl>>(parsed_arr.get_error());
        }
        auto array = std::move(parsed_arr.unwrap());
        // is ui_control array
        if(peek().type == token::OPEN_BRACKET) {
            consume(); // consume [
            auto sizes = parse_param_list(node_ui_control.get());
            if(sizes.is_error()) {
                return Result<std::unique_ptr<NodeUIControl>>(sizes.get_error());
            }
            control_array_sizes = std::move(sizes.unwrap());
            if(peek().type != token::CLOSED_BRACKET) {
                return Result<std::unique_ptr<NodeUIControl>>(CompileError(ErrorType::SyntaxError,
                "Expected closing bracket after ui_control array size.", "]", peek()));
            }
            consume(); // consume ]

        } else {
            control_array_sizes = std::unique_ptr<NodeParamList>(new NodeParamList({}, get_tok()));
        }
        control_var = std::move(array);
    } else {
        auto parsed_var = parse_variable(node_ui_control.get(), persistence, var_type);
        if(parsed_var.is_error()) {
            return Result<std::unique_ptr<NodeUIControl>>(parsed_var.get_error());
        }
        control_var = std::move(parsed_var.unwrap());
    }
    control_var -> is_reference = false;
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
    node_ui_control->ui_control_type = std::move(ui_control_type);
    node_ui_control->control_var = std::move(control_var);
    node_ui_control->params = std::move(control_params);
    node_ui_control->sizes = std::move(control_array_sizes);
    node_ui_control->set_child_parents();
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

    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
         "Expected linebreak after if-condition.", "linebreak", peek()));
    }
	consume(); // consume linebreak
	_skip_linebreaks();
	auto if_statements = std::make_unique<NodeBody>(get_tok());
    while (peek().type != token::END_IF && peek().type != token::ELSE) {
		if(peek().type == token::END_IF or peek().type == token::ELSE) break;
        auto stmt = parse_statement(node_if_statement.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            if_statements->statements.push_back(std::move(stmt.unwrap()));
    }
    bool no_end_if = false;
	auto else_statements = std::make_unique<NodeBody>(get_tok());
	else_statements->parent = node_if_statement.get();
    if(peek().type == token::ELSE) {
        consume();
        if(not(peek().type == token::IF || peek().type == token::LINEBRK)) {
            return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
            "Expected linebreak after else-condition.", "linebreak", peek()));
        }
        if(peek().type == token::IF) {
            no_end_if = true;
            auto stmt = parse_if_statement(node_if_statement.get());
            if (stmt.is_error()) {
                return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
            }
            auto stmt_val = std::make_unique<NodeStatement>(std::move(stmt.unwrap()), get_tok());
			else_statements->statements.push_back(std::move(stmt_val));
        } else {
            while (peek().type != token::END_IF) {
                auto stmt = parse_statement(node_if_statement.get());
                if (stmt.is_error()) {
                    return Result<std::unique_ptr<NodeIfStatement>>(stmt.get_error());
                }
                if(stmt.unwrap()->statement)
					else_statements->statements.push_back(std::move(stmt.unwrap()));
            }
        }
    }
    if(not(peek().type == token::END_IF || no_end_if)) {
        return Result<std::unique_ptr<NodeIfStatement>>(CompileError(ErrorType::SyntaxError,
     "Missing end token.", "end if",peek()));
    }
    if (!no_end_if) consume();
    node_if_statement->condition = std::move(condition);
    node_if_statement->statements = std::move(if_statements);
    node_if_statement->else_statements = std::move(else_statements);
    node_if_statement->set_child_parents();
    node_if_statement->parent = parent;
    return Result<std::unique_ptr<NodeIfStatement>>(std::move(node_if_statement));
}

Result<std::unique_ptr<NodeForStatement>> Parser::parse_for_statement(NodeAST* parent) {
    auto node_for_statement = std::make_unique<NodeForStatement>(get_tok());
    //consume for
    consume();
    auto assign_stmt = parse_single_assign_statement(node_for_statement.get());
    if(assign_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForStatement>>(assign_stmt.get_error());
    }
    auto iterator = std::move(assign_stmt.unwrap());
    if(not(peek().type == token::TO || peek().type == token::DOWNTO)) {
        return Result<std::unique_ptr<NodeForStatement>>(CompileError(ErrorType::SyntaxError,
        "Incorrect <for-loop> Syntax.", "to/downto", peek()));
    }
    Token to = consume(); //consume to or downto
    auto expression_stmt = parse_binary_expr(node_for_statement.get());
    if(expression_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForStatement>>(expression_stmt.get_error());
    }
    auto iterator_end = std::move(expression_stmt.unwrap());
    std::unique_ptr<NodeAST> step;
    if(peek().type == token::STEP) {
        consume(); // consume step
        auto step_expression = parse_binary_expr(node_for_statement.get());
        if(step_expression.is_error()) {
            return Result<std::unique_ptr<NodeForStatement>>(step_expression.get_error());
        }
        step = std::move(step_expression.unwrap());
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeForStatement>>(CompileError(ErrorType::SyntaxError,
         "Missing linebreak in <for-loop>", "linebreak", peek()));
    }
    consume(); //consume linebreak
    auto node_body = std::make_unique<NodeBody>(get_tok());
    while (peek().type != token::END_FOR) {
        _skip_linebreaks();
        if(peek().type == token::END_FOR) break;
        auto stmt = parse_statement(node_body.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeForStatement>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            node_body->statements.push_back(std::move(stmt.unwrap()));
    }
    consume(); // consume end for
    node_for_statement->iterator = std::move(iterator);
    node_for_statement->to = to;
    node_for_statement->iterator_end = std::move(iterator_end);
    node_for_statement->statements = std::move(node_body);
    node_for_statement->step = std::move(step);
    node_for_statement->set_child_parents();
    node_for_statement->parent = parent;
    return Result<std::unique_ptr<NodeForStatement>>(std::move(node_for_statement));
}

bool Parser::is_ranged_for_loop() {
    size_t begin = m_pos;
    while(peek().type != token::LINEBRK) {
        if(peek().type == token::ASSIGN) {
            m_pos = begin;
            return false;
        }
        else if(peek().type == token::IN){
            m_pos = begin;
            return true;
        }
        else {
            consume();
        }
    }
    return false;
}

Result<std::unique_ptr<NodeForEachStatement>> Parser::parse_for_each_statement(NodeAST* parent) {
    auto node_for_statement = std::make_unique<NodeForEachStatement>(get_tok());
    //consume for
    consume();
    auto key_value_result = parse_param_list(node_for_statement.get());
    if(key_value_result.is_error())
        Result<std::unique_ptr<NodeForEachStatement>>(key_value_result.get_error());
    if(peek().type != token::IN)
        return Result<std::unique_ptr<NodeForEachStatement>>(CompileError(ErrorType::SyntaxError,
                                                                          "Incorrect Syntax for range-based <for-loop>.", "in", peek()));
    Token to = consume(); //consume in
    auto expression_stmt = parse_binary_expr(node_for_statement.get());
    if(expression_stmt.is_error()) {
        return Result<std::unique_ptr<NodeForEachStatement>>(expression_stmt.get_error());
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeForEachStatement>>(CompileError(ErrorType::SyntaxError,
                                                                          "Missing linebreak in <for-loop>", "linebreak", peek()));
    }
    consume(); //consume linebreak
    auto node_body = std::make_unique<NodeBody>(get_tok());
    while (peek().type != token::END_FOR) {
        _skip_linebreaks();
        if(peek().type == token::END_FOR) break;
        auto stmt = parse_statement(node_body.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeForEachStatement>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            node_body->statements.push_back(std::move(stmt.unwrap()));
    }
    consume(); // consume end for
    node_for_statement->keys = std::move(key_value_result.unwrap());
    node_for_statement->range = std::move(expression_stmt.unwrap());
    node_for_statement->statements = std::move(node_body);
    node_for_statement->set_child_parents();
    node_for_statement->parent = parent;
    return Result<std::unique_ptr<NodeForEachStatement>>(std::move(node_for_statement));
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
         "While Statement needs condition.", peek().line, "condition", condition->get_string(), peek().file));
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeWhileStatement>>(CompileError(ErrorType::ParseError,
         "Expected linebreak after while-condition.", "linebreak", peek()));
    }
    consume(); //consume linebreak
    auto node_body = std::make_unique<NodeBody>(get_tok());
    while (peek().type != token::END_WHILE) {
        auto stmt = parse_statement(node_body.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeWhileStatement>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            node_body->statements.push_back(std::move(stmt.unwrap()));
    }
    consume(); // consume end while
    node_while_statement->condition = std::move(condition);
    node_while_statement->statements = std::move(node_body);
    node_while_statement->set_child_parents();
    node_while_statement ->parent = parent;
    return Result<std::unique_ptr<NodeWhileStatement>>(std::move(node_while_statement));
}

Result<std::unique_ptr<NodeSelectStatement>> Parser::parse_select_statement(NodeAST* parent) {
    auto node_select_statement = std::make_unique<NodeSelectStatement>(get_tok());
	consume(); //consume select
	auto expression = parse_expression(node_select_statement.get());
	if(peek().type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		"Expected linebreak after select-expression.", "linebreak", peek()));
	}
	consume(); //consume linebreak
	_skip_linebreaks();
	if(peek().type != token::CASE) {
		return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
		 "Expected cases in select-expression.", "case <expression>", peek()));
	}
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBody>>> cases;
	while (peek().type != token::END_SELECT) {
		_skip_linebreaks();
		if(peek().type == token::CASE) {
			consume(); //consume case
            std::vector<std::unique_ptr<NodeAST>> cas = {};
			if(peek().type == token::DEFAULT) {
				auto default_token = consume(); // consume default token
				Token low_end = Token(token::INT, "080000000H", default_token.line,default_token.pos, default_token.file);
				Token high_end = Token(token::INT, "07FFFFFFH", default_token.line,default_token.pos, default_token.file);
				auto node_int_low = std::move(parse_int(low_end, 16, node_select_statement.get()).unwrap());
				cas.push_back(std::move(node_int_low));
				auto node_int_high = std::move(parse_int(high_end, 16, node_select_statement.get()).unwrap());
				cas.push_back(std::move(node_int_high));
			} else {
				auto cas_result = parse_expression(node_select_statement.get());
				if (cas_result.is_error())
					return Result<std::unique_ptr<NodeSelectStatement>>(cas_result.get_error());
				cas.push_back(std::move(cas_result.unwrap()));
				if (peek().type == token::TO) {
					consume(); // consume to
					auto cas2_result = parse_expression(node_select_statement.get());
					if (cas2_result.is_error())
						return Result<std::unique_ptr<NodeSelectStatement>>(cas2_result.get_error());
					cas.push_back(std::move(cas2_result.unwrap()));
				}
			}
			if(peek().type != token::LINEBRK) {
				return Result<std::unique_ptr<NodeSelectStatement>>(CompileError(ErrorType::SyntaxError,
				 "Expected linebreak after case.", "linebreak", peek()));
			}
			consume(); //consume linebreak
            auto stmts = std::make_unique<NodeBody>(get_tok());
//            stmts->parent = node_select_statement.get();
			while(peek().type != token::END_SELECT && peek().type != token::CASE) {
				auto stmt = parse_statement(node_select_statement.get());
				if (stmt.is_error()) {
					return Result<std::unique_ptr<NodeSelectStatement>>(stmt.get_error());
				}
                if(stmt.unwrap()->statement) {
                    stmt.unwrap()->parent = stmts.get();
                    stmts->statements.push_back(std::move(stmt.unwrap()));
                }
			}
			cases.emplace_back(std::move(cas),std::move(stmts));
		}
	}
	consume(); // consume end select
    node_select_statement->expression = std::move(expression.unwrap());
    node_select_statement->cases = std::move(cases);
    node_select_statement->set_child_parents();
    node_select_statement->parent = parent;
	return Result<std::unique_ptr<NodeSelectStatement>>(std::move(node_select_statement));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_family_statement(NodeAST* parent) {
	auto node_family_statement = std::make_unique<NodeFamilyStatement>(get_tok());
	Token construct = consume(); //consume family
	token end_construct = token::END_FAMILY;
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
		 "Found unknown family syntax.", "valid prefix", peek()));
	}
	auto prefix = consume(); //consume prefix
	auto l = consume_linebreak("<family statement>");
	if(l.is_error())
		return Result<std::unique_ptr<NodeAST>>(l.get_error());
    auto node_body = std::make_unique<NodeBody>(construct);
//	std::vector<std::unique_ptr<NodeStatement>> stmts;
	while(peek().type != token::END_FAMILY) {
		_skip_linebreaks();
		auto declare_stmt = parse_statement(nullptr);
		if(declare_stmt.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(declare_stmt.get_error());
		}
		declare_stmt.unwrap() -> parent = node_body.get();
        if(declare_stmt.unwrap()->statement)
		    node_body->statements.push_back(std::move(declare_stmt.unwrap()));
	}
	consume(); // consume end family
	node_family_statement->prefix = prefix.val;
	node_family_statement->members = std::move(node_body);
    node_family_statement->set_child_parents();
	node_family_statement -> parent = parent;
	return Result<std::unique_ptr<NodeAST>>(std::move(node_family_statement));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_list_block(NodeAST* parent) {
    auto node_list_block = std::make_unique<NodeListStruct>(get_tok());
    Token construct = consume(); //consume list
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
     "Found unknown <list> syntax.", "valid <keyword>", peek()));
    }
    Token name_tok = consume(); // consume keyword
    std::string name = name_tok.val;
	auto type = infer_type_from_identifier(name);
    if(peek().type != token::OPEN_BRACKET) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
     "Found unknown <list> syntax.", "[", peek()));
    }
    consume(); // consume [
    if(peek().type == token::COMMA) {
        consume(); // consume comma
    }
    if(peek().type != token::CLOSED_BRACKET) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
     "Found unknown <list> syntax.", "]", peek()));
    }
    consume(); // consume ]
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
         "Expected linebreak.", "linebreak", peek()));
    }
    consume(); // consume linebreak
    std::vector<std::unique_ptr<NodeParamList>> stmts;
    int32_t size = 0;
    while(peek().type != token::END_LIST) {
        _skip_linebreaks();
        if(peek().type == token::END_LIST) break;
        auto param_list = parse_param_list(node_list_block.get());
        if(param_list.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(param_list.get_error());
        }
        size += (int32_t)param_list.unwrap()->params.size();
        stmts.push_back(std::move(param_list.unwrap()));
        auto l = consume_linebreak("<statement>");
        if(l.is_error())
            return Result<std::unique_ptr<NodeAST>>(l.get_error());
    }
    consume(); // consume end_list
    node_list_block->name = name;
    node_list_block->size = size;
    node_list_block->body = std::move(stmts);
    node_list_block->parent = parent;
	node_list_block->type = type;
    return Result<std::unique_ptr<NodeAST>>(std::move(node_list_block));
}


Result<std::unique_ptr<NodeAST>> Parser::parse_const_statement(NodeAST* parent) {
    auto node_const_statement = std::make_unique<NodeConstStatement>(get_tok());
    Token construct = consume(); //consume family, struct, const
    token end_construct = token::END_CONST;
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
            "Found unknown const syntax.", "valid prefix", peek()));
    }
    auto prefix = consume(); //consume prefix

    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
            "Expected linebreak.", "linebreak", peek()));
    }
    consume(); // consume linebreak

    auto node_body = std::make_unique<NodeBody>(construct);
    while(peek().type != end_construct) {
        _skip_linebreaks();
        if(peek().type == end_construct) break;
        if(peek().type == token::DECLARE) {
            return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
             "Found unknown const syntax.", "const variable", peek()));
        }
        auto const_stmt = parse_declare_statement(node_const_statement.get());
        if(const_stmt.is_error()) {
            return Result<std::unique_ptr<NodeAST>>(const_stmt.get_error());
        }
		auto node_stmt = std::make_unique<NodeStatement>(std::move(const_stmt.unwrap()), get_tok());
        node_stmt->parent = node_body.get();
        node_body->statements.push_back(std::move(node_stmt));
		auto l = consume_linebreak("<statement>");
		if(l.is_error())
			return Result<std::unique_ptr<NodeAST>>(l.get_error());
    }
    consume(); // consume end_const

    node_const_statement -> parent = parent;
    node_const_statement->name = prefix.val;
    node_const_statement->constants = std::move(node_body);
    node_const_statement->set_child_parents();
	// set the parent for each statement in stmts
    return Result<std::unique_ptr<NodeAST>>(std::move(node_const_statement));
}

Result<SuccessTag> Parser::consume_linebreak(const std::string& construct) {
	if(peek().type != token::LINEBRK) {
		return Result<SuccessTag>(CompileError(ErrorType::SyntaxError,
		 "Missing linebreak in "+construct+".", "linebreak", peek()));
	}
	consume(); // consume linebreak
	return Result<SuccessTag>(SuccessTag{});
}


Result<std::unique_ptr<NodeGetControlStatement>> Parser::parse_get_control_statement(std::unique_ptr<NodeAST> ui_id, NodeAST* parent) {
    if(peek().type != token::ARROW) {
        return Result<std::unique_ptr<NodeGetControlStatement>>(CompileError(ErrorType::SyntaxError,
     "Wrong control statement syntax.", "->", peek()));
    }
    consume(); // consume ->
	if(peek().type == token::DEFAULT) {
		m_tokens[m_pos].type = token::KEYWORD;
	}
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeGetControlStatement>>(CompileError(ErrorType::SyntaxError,
         "Wrong control statement syntax.", "<control_parameter>", peek()));
    }
    auto control_param = consume().val;
    auto node_get_control_statement = std::make_unique<NodeGetControlStatement>(std::move(ui_id), control_param, get_tok());
	node_get_control_statement->set_child_parents();
	node_get_control_statement->parent = parent;
    return Result<std::unique_ptr<NodeGetControlStatement>>(std::move(node_get_control_statement));
}

void Parser::mark_function_as_used(const std::string& func_name, int num_args) {
    auto it = m_function_definitions.find({func_name, num_args});
    if(it != m_function_definitions.end()) {
        it->second->is_used = true;
    }
}





