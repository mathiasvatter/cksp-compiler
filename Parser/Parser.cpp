//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"

Parser::Parser(std::vector<Token> tokens): Processor(std::move(tokens)) {}

Result<std::unique_ptr<NodeProgram>> Parser::parse() {
	return parse_program();
}

std::string Parser::sanitize_binary(const std::string& input) {
    if (input.empty()) {
        return input;
    }
    std::string sanitized = input;
    // Entferne das 'b' oder 'B' am Anfang oder Ende und kehre die Zeichenfolge um, falls nötig
    if (input[0] == 'b' || input[0] == 'B') {
        sanitized = input.substr(1);
        std::ranges::reverse(sanitized);
    } else if (input.back() == 'b' || input.back() == 'B') {
        sanitized = input.substr(0, input.size() - 1);
    }
    return sanitized;
}

std::string Parser::sanitize_hex(const std::string& input) {
    // Überprüfen, ob der String mit einer Ziffer zwischen 0 und 9 beginnt und mit "h" endet
    if (input.size() > 1 && isdigit(input[0]) && (input.back() == 'h' || input.back() == 'H')) {
        // Entfernen der ersten Ziffer und des letzten "h"
        std::string newStr = input.substr(1, input.size() - 2);
        // Hinzufügen von "0x" am Anfang
        return newStr;
    }
    // Wenn die Bedingungen nicht erfüllt sind, den ursprünglichen String zurückgeben
    return input;
}

Result<std::unique_ptr<NodeInt>> Parser::parse_int(const Token& tok, const int base, NodeAST* parent) {
    auto value = tok.val;
    if(base == 2)
        value = sanitize_binary(value);
    else if (base == 16) {
        value = sanitize_hex(value);
    }
    try {
        const long long val = std::stoll(value, nullptr, base);
        auto node_int = std::make_unique<NodeInt>(static_cast<int32_t>(val & 0xFFFFFFFF), tok);
        node_int->parent= parent;
        return Result<std::unique_ptr<NodeInt>>(std::move(node_int));
    } catch (const std::exception& e) {
        const auto expected = std::string(1, "valid int base "[base]);
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

Result<std::unique_ptr<NodeFormatString>> Parser::parse_fstring(NodeAST *parent) {
	auto start_tok = consume(); // consume 'f"'

	auto fstring_node = std::make_unique<NodeFormatString>(start_tok);
	while (peek().type != token::FSTRING_STOP) {
		if (peek().type == token::FSTRING_EXPR_START) {
			consume(); // consume '<'
			auto expr = parse_expression(parent);
			if (expr.is_error()) {
				return Result<std::unique_ptr<NodeFormatString>>(expr.get_error());
			}
			fstring_node->add_element(std::move(expr.unwrap()));
			if (peek().type != token::FSTRING_EXPR_STOP) {
				return Result<std::unique_ptr<NodeFormatString>>(
					CompileError(ErrorType::ParseError, "Expected '>' to close format string expression.", ">", peek()));
			}
			consume(); // consume '>'
		} else if (peek().type == token::STRING) {
			auto fstring = parse_string(parent);
			if (fstring.is_error()) {
				return Result<std::unique_ptr<NodeFormatString>>(fstring.get_error());
			}
			fstring_node->add_element(std::move(fstring.unwrap()));
		} else {
			auto error = CompileError(
				ErrorType::ParseError,
				"Expected a string or format string expression inside format string.",
				"string or format string expression",
				peek());
			return Result<std::unique_ptr<NodeFormatString>>(error);
		}
	}
	auto quotes = consume(); // consume fstring end
	fstring_node->parent = parent;
	fstring_node->quotes = quotes.val;
	return Result<std::unique_ptr<NodeFormatString>>(std::move(fstring_node));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_nil(NodeAST* parent) {
	auto nil = consume(); // consume 'nil'
	auto node_nil = std::make_unique<NodeNil>(nil);
	node_nil->parent = parent;
	return Result<std::unique_ptr<NodeAST>>(std::move(node_nil));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_wildcard(NodeAST* parent) {
	auto wildcard = consume();
	auto node_wildcard = std::make_unique<NodeWildcard>(wildcard.val, wildcard);
	node_wildcard->parent = parent;
	return Result<std::unique_ptr<NodeAST>>(std::move(node_wildcard));
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
            auto node_real = std::make_unique<NodeReal>(std::stod(value.val), value);
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
	auto ty = TypeRegistry::get_type_from_identifier(var_name[0]);
	if(ty != TypeRegistry::Unknown) var_name = var_name.erase(0,1);
    auto node_variable = std::make_unique<NodeVariable>(is_persistent, var_name, ty, var_token, var_type);
    node_variable->parent = parent;
    return Result<std::unique_ptr<NodeVariable>>(std::move(node_variable));
}

Result<std::unique_ptr<NodeVariableRef>> Parser::parse_variable_ref(NodeAST* parent) {
	auto var_token = consume();
	std::string var_name = var_token.val;
	const auto ty = TypeRegistry::get_type_from_identifier(var_name[0]);
	if(ty != TypeRegistry::Unknown) var_name = var_name.erase(0,1);
	auto node_variable_ref = std::make_unique<NodeVariableRef>(var_name, var_token);
	node_variable_ref->parent = parent;
	node_variable_ref->ty = ty;
	return Result<std::unique_ptr<NodeVariableRef>>(std::move(node_variable_ref));
}

Result<std::unique_ptr<NodePointer>> Parser::parse_pointer(NodeAST* parent, const std::optional<Token>& is_persistent) {
	auto ptr_token = consume();
	std::string ptr_name = ptr_token.val;
	auto ty = TypeRegistry::get_type_from_identifier(ptr_name[0]);
	if(ty != TypeRegistry::Unknown) {
		auto error = CompileError(ErrorType::SyntaxError,"Found unknown Pointer Syntax.", "", peek());
		error.m_message += " Pointer cannot have <vanilla KSP> identifiers.";
		error.m_got = ptr_name[0];
		error.m_expected = "valid annotation syntax, e.g. '"+ptr_name+" : <Object>'";
		error.exit();
	}
	auto node_pointer = std::make_unique<NodePointer>(is_persistent, ptr_name, ty, ptr_token);
	node_pointer->parent = parent;
	return Result<std::unique_ptr<NodePointer>>(std::move(node_pointer));
}

Result<std::unique_ptr<NodePointerRef>> Parser::parse_pointer_ref(NodeAST* parent) {
	auto ptr_token = consume();
	std::string ptr_name = ptr_token.val;
	auto ty = TypeRegistry::get_type_from_identifier(ptr_name[0]);
	if(ty != TypeRegistry::Unknown) {
		auto error = CompileError(ErrorType::SyntaxError,"Found unknown Pointer Syntax.", "", peek());
		error.m_message += " Pointer Reference cannot have <vanilla KSP> identifiers.";
		error.m_got = ptr_name[0];
		error.exit();
	}
	auto node_pointer_ref = std::make_unique<NodePointerRef>(ptr_name, ptr_token);
	node_pointer_ref->parent = parent;
	node_pointer_ref->ty = ty;
	return Result<std::unique_ptr<NodePointerRef>>(std::move(node_pointer_ref));
}

Result<std::unique_ptr<NodeDataStructure>> Parser::parse_array(NodeAST *parent, std::optional<Token> is_persistent, DataType var_type) {
	auto error = CompileError(ErrorType::SyntaxError,"Found unknown Array Syntax.", "", peek());
    auto arr_token = consume();
    std::string arr_name = arr_token.val;
	auto ty = TypeRegistry::get_type_from_identifier(arr_name[0]);
	if(ty != TypeRegistry::Unknown) arr_name = arr_name.erase(0,1);
	std::unique_ptr<NodeDataStructure> node_array = nullptr;
	std::unique_ptr<NodeParamList> sizes = std::make_unique<NodeParamList>(arr_token);
    sizes->parent = node_array.get();
    if(peek().type == token::OPEN_BRACKET) {
        consume(); // consume [
        // if it is an empty array initialization
        if (peek().type != token::CLOSED_BRACKET) {
            auto sizes_params = parse_multiple_values(node_array.get());
            if (sizes_params.is_error()) {
                return Result<std::unique_ptr<NodeDataStructure>>(sizes_params.get_error());
            }
            sizes = std::move(sizes_params.unwrap());
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

	// if it is multidimensional array
	if(sizes->params.size() > 1) {
		auto node = std::make_unique<NodeNDArray>(
			std::move(is_persistent),
			arr_name, ty,
			std::move(sizes), arr_token
		);
		node->dimensions = node->sizes->params.size();
		// provide type dimensions here because eg '!' can be used for nd-arrays but are usually for 1 dimension only
		if(ty->get_type_kind() == TypeKind::Composite) {
			auto nd_type = static_cast<CompositeType*>(ty);
			node->ty = TypeRegistry::add_composite_type(nd_type->get_compound_type(), nd_type->get_element_type(), node->dimensions);
		}
		node_array = std::move(node);
	} else {
		auto node = std::make_unique<NodeArray>(arr_name, arr_token);
		node->persistence = std::move(is_persistent);
		node->data_type = var_type;
        node->ty = ty;
		if(!sizes->params.empty()) node->size = std::move(sizes->params[0]);
		node_array = std::move(node);
		node_array->set_child_parents();
	}

	node_array->parent = parent;
	return Result<std::unique_ptr<NodeDataStructure>>(std::move(node_array));
}

Result<std::unique_ptr<NodeReference>> Parser::parse_array_ref(NodeAST *parent) {
	auto error = CompileError(ErrorType::SyntaxError,"Found unknown Array Reference Syntax.", "", peek());
	auto arr_token = consume();
	std::string arr_name = arr_token.val;
	auto ty = TypeRegistry::get_type_from_identifier(arr_name[0]);
	if(ty != TypeRegistry::Unknown) arr_name = arr_name.erase(0,1);
	std::unique_ptr<NodeReference> node_array_ref = nullptr;
	auto indexes = std::make_unique<NodeParamList>(arr_token);;
	indexes->parent = node_array_ref.get();
	if(peek().type == token::OPEN_BRACKET) {
		consume(); // consume [
		// if it is an empty array initialization
		if (peek().type == token::CLOSED_BRACKET) {
			error.m_message += "Empty Array Reference Initialization is not allowed.";
			return Result<std::unique_ptr<NodeReference>>(error);
		}
		auto index_params = parse_multiple_values(node_array_ref.get());
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
	node_array_ref->ty = ty;
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
	    if (auto expr = parse_string(parent); !expr.is_error()) {
	    	return Result<std::unique_ptr<NodeAST>>(std::move(expr.unwrap()));
	    } else return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    } else if (peek().type == token::FSTRING_START) {
    	if (auto expr = parse_fstring(parent); !expr.is_error()) {
    		return Result<std::unique_ptr<NodeAST>>(std::move(expr.unwrap()));
    	} else return Result<std::unique_ptr<NodeAST>>(expr.get_error());
    } else {
	    if (auto expr = parse_binary_expr(parent); !expr.is_error()) {
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
        lhs->ty = TypeRegistry::String;
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_binary_expr(NodeAST* parent) {
    auto lhs = _parse_primary_expr(parent);
    if(!lhs.is_error()) {
        return _parse_binary_expr_rhs(0, std::move(lhs.unwrap()), parent);
    }
    return Result<std::unique_ptr<NodeAST>>(lhs.get_error());
}

Result<std::unique_ptr<NodeAST>> Parser::parse_reference_chain(NodeAST *parent) {
	auto chain = std::make_unique<NodeAccessChain>(peek());
	while(peek().type == token::KEYWORD) {
		std::unique_ptr<NodeAST> stmt = nullptr;
		if (peek().type == token::KEYWORD) {
			// is function
			if (peek(1).type == token::OPEN_PARENTH) {
				auto var_function = parse_function_call(parent);
				if (var_function.is_error()) {
					return Result<std::unique_ptr<NodeAST>>(var_function.get_error());
				}
				stmt = std::move(var_function.unwrap());
			} else if (peek(1).type == token::OPEN_BRACKET) {
				auto var_array = parse_array_ref(parent);
				if (var_array.is_error()) {
					return Result<std::unique_ptr<NodeAST>>(var_array.get_error());
				}
				if (peek().type == token::ARROW) {
					auto get_control = parse_get_control_statement(std::move(var_array.unwrap()), parent);
					if (get_control.is_error())
						return Result<std::unique_ptr<NodeAST>>(get_control.get_error());
					stmt = std::move(get_control.unwrap());
				} else {
					stmt = std::move(var_array.unwrap());
				}
			} else {
				// is variable
				auto var = parse_variable_ref(parent);
				if (var.is_error()) {
					return Result<std::unique_ptr<NodeAST>>(var.get_error());
				}
				if (peek().type == token::ARROW) {
					auto get_control = parse_get_control_statement(std::move(var.unwrap()), parent);
					if (get_control.is_error())
						return Result<std::unique_ptr<NodeAST>>(get_control.get_error());
					stmt = std::move(std::move(get_control.unwrap()));
				} else {
					stmt = std::move(std::move(var.unwrap()));
				}
			}
		}
		chain->add_method(std::move(stmt));
		if(peek().type == token::DOT) {
			consume();
		} else {
			break;
		}
	}
	if(chain->chain.size() == 1) {
		chain->chain[0]->parent = parent;
		return Result<std::unique_ptr<NodeAST>>(std::move(chain->chain[0]));
	} else {
		chain->parent = parent;
		return Result<std::unique_ptr<NodeAST>>(std::move(chain));
	}
}


Result<std::unique_ptr<NodeAST>> Parser::_parse_primary_expr(NodeAST* parent) {
	if(peek().type == token::RETURN) {
		m_tokens[m_pos].type = token::KEYWORD;
	}
    if(peek().type == token::KEYWORD) {
		return parse_reference_chain(parent);
    // is expression in brackets
    } else if (peek().type == token::OPEN_PARENTH) {
        return _parse_parenth_expr(parent);
    } else if (peek().type == token::INT || peek().type == token::FLOAT || peek().type == token::HEXADECIMAL || peek().type == token::BINARY) {
        return parse_number(parent);
    // unary operators bool_not, bit_not, sub
    } else if (UNARY_TOKENS.contains(peek().type)) {
	    return parse_unary_expr(parent);
    } else if (peek().type == token::OPEN_BRACKET) {
    	return parse_init_list(parent);
	} else if (peek().type == token::NIL) {
		return parse_nil(parent);
	} else if (peek().type == token::NEW) {
		auto var_method = parse_function_call(parent);
		if (!var_method.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(std::move(var_method.unwrap()));
		}
		return Result<std::unique_ptr<NodeAST>>(var_method.get_error());
	} else if(peek().type == token::MULT) {
		return parse_wildcard(parent);
    } else {
        return Result<std::unique_ptr<NodeAST>>(
    CompileError(ErrorType::ParseError,"Found unknown expression token.", "keyword, integer, parenthesis", peek()));
    }
}

Result<std::unique_ptr<NodeAST>> Parser::parse_unary_expr(NodeAST* parent) {
    auto node_unary_expr = std::make_unique<NodeUnaryExpr>(get_tok());
    const Token unary_op = consume();
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


Result<std::unique_ptr<NodeAST>> Parser::_parse_binary_expr_rhs(const int precedence, std::unique_ptr<NodeAST> lhs, NodeAST* parent) {
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
        if (const int next_precedence = _get_binop_precedence(peek().type); prec < next_precedence) {
            rhs = _parse_binary_expr_rhs(prec + 1, std::move(rhs.unwrap()), parent);
            if (rhs.is_error()) {
                return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
            }
        }
        Type* type = TypeRegistry::Unknown;
		if (COMPARISON_TOKENS.contains(bin_op.type)) {
            // Check if rhs is NodeComparisonExpr because comparisons in comparisons are not allowed
            if (lhs->ty == TypeRegistry::Comparison) {
                return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
                 "Nested Comparisons are not allowed.", "valid expression operator", bin_op));
            }
            type = TypeRegistry::Comparison;
		} else if (bin_op.type == token::BOOL_AND || bin_op.type == token::BOOL_OR){
			type = TypeRegistry::Boolean;
		}

		auto node_binary_expr = std::make_unique<NodeBinaryExpr>(bin_op.type, std::move(lhs), std::move(rhs.unwrap()), get_tok());
        node_binary_expr->parent = parent;
        lhs = std::move(node_binary_expr);
        lhs->ty = type;
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

Result<std::unique_ptr<NodeFunctionParam>> Parser::parse_function_param(NodeAST* parent) {
	auto node_func_param = std::make_unique<NodeFunctionParam>(get_tok());
	if (peek().type == token::REF) {
		consume(); // consume ref
		node_func_param->is_pass_by_ref = true;
	}

	auto error = CompileError(ErrorType::ParseError,"Incorrect syntax in function parameter.", "", peek());
	if(peek().type != token::KEYWORD) {
		error.m_expected = "<variable>, <array>";
		return Result<std::unique_ptr<NodeFunctionParam>>(error);
	}

	if (is_array_declaration()) {
		auto parsed_arr = parse_declare_array(node_func_param.get());
		if(parsed_arr.is_error()) {
			return Result<std::unique_ptr<NodeFunctionParam>>(parsed_arr.get_error());
		}
		node_func_param->variable = std::move(parsed_arr.unwrap());
		// variable
	} else if(is_variable_declaration()) {
		auto parsed_var = parse_declare_variable(node_func_param.get());
		if (parsed_var.is_error()) {
			return Result<std::unique_ptr<NodeFunctionParam>>(parsed_var.get_error());
		}
		node_func_param->variable = std::move(parsed_var.unwrap());
	} else {
		error.m_expected = "<variable>, <array>";
		return Result<std::unique_ptr<NodeFunctionParam>>(error);
	}

	std::unique_ptr<NodeAST> r_value = nullptr;
	// if there is an assignment following
	if (peek().type == token::ASSIGN) {
		consume(); //consume :=
		if (peek().type == token::OPEN_PARENTH) {
			size_t backup_pos = m_pos; // backup token index

			if (auto exprResult = parse_expression(node_func_param.get()); !exprResult.is_error()) {
				// if start was "(" and before  was ")" -> param_list
				if(peek(-1).type == token::CLOSED_PARENTH) {
					auto param_list = std::make_unique<NodeParamList>(get_tok(), std::move(exprResult.unwrap()));
					param_list ->parent = node_func_param.get();
					r_value = std::move(param_list);
				} else {
					r_value = std::move(exprResult.unwrap());
				}
			} else {
				return Result<std::unique_ptr<NodeFunctionParam>>(exprResult.get_error());
			}
		} else {
			if (auto exprResult = parse_expression(node_func_param.get()); !exprResult.is_error()) {
				r_value = std::move(exprResult.unwrap());
			} else {
				return Result<std::unique_ptr<NodeFunctionParam>>(exprResult.get_error());
			}
		}

	}
	node_func_param->value = std::move(r_value);
	node_func_param->set_child_parents();
	node_func_param->parent = parent;
	return Result<std::unique_ptr<NodeFunctionParam>>(std::move(node_func_param));
}


Result<std::unique_ptr<NodeSingleDeclaration>> Parser::parse_single_declare_statement(NodeAST* parent) {
	auto node_declare_statement = std::make_unique<NodeSingleDeclaration>(get_tok());
	if(peek().type == token::DECLARE) consume(); //consume declare
	if(!modifier_keywords.contains(peek().type) and peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeSingleDeclaration>>(CompileError(ErrorType::ParseError,
																	 "Incorrect syntax in declare statement. Found unknown <modifier>.", "<ui_control>, <variable>, <array>", peek()));
	}
	// ui_control
	if (peek().type == token::UI_CONTROL xor peek(1).type == token::UI_CONTROL) {
		auto parsed_ui_control = parse_declare_ui_control(node_declare_statement.get());
		if (parsed_ui_control.is_error()) {
			return Result<std::unique_ptr<NodeSingleDeclaration>>(parsed_ui_control.get_error());
		}
		node_declare_statement->variable = std::move(parsed_ui_control.unwrap());
		// array
	} else if (is_array_declaration()) {
		auto parsed_arr = parse_declare_array(node_declare_statement.get());
		if(parsed_arr.is_error()) {
			return Result<std::unique_ptr<NodeSingleDeclaration>>(parsed_arr.get_error());
		}
		node_declare_statement->variable = std::move(parsed_arr.unwrap());
		// variable
	} else if(is_variable_declaration()) {
		auto parsed_var = parse_declare_variable(node_declare_statement.get());
		if (parsed_var.is_error()) {
			return Result<std::unique_ptr<NodeSingleDeclaration>>(parsed_var.get_error());
		}
		node_declare_statement->variable = std::move(parsed_var.unwrap());
	} else {
		return Result<std::unique_ptr<NodeSingleDeclaration>>(CompileError(ErrorType::ParseError,
																	 "Incorrect syntax in declare statement.", "<ui_control>, <variable>, <array>", peek()));
	}

	std::unique_ptr<NodeAST> r_value = nullptr;
	// if there is an assignment following
	if (peek().type == token::ASSIGN) {
		consume(); //consume :=
		if (peek().type == token::OPEN_PARENTH) {
			size_t backup_pos = m_pos; // backup token index
			auto exprResult = parse_expression(node_declare_statement.get());

			if (!exprResult.is_error()) {
				// if start was "(" and before  was ")" -> param_list
				if(peek(-1).type == token::CLOSED_PARENTH) {
					auto param_list = std::make_unique<NodeParamList>(get_tok(), std::move(exprResult.unwrap()));
					param_list ->parent = node_declare_statement.get();
					r_value = std::move(param_list);
				} else {
					r_value = std::move(exprResult.unwrap());
				}
			} else {
				return Result<std::unique_ptr<NodeSingleDeclaration>>(exprResult.get_error());
			}
		} else {
			if (auto exprResult = parse_expression(node_declare_statement.get()); !exprResult.is_error()) {
				r_value = std::move(exprResult.unwrap());
			} else {
				return Result<std::unique_ptr<NodeSingleDeclaration>>(exprResult.get_error());
			}
		}

	}
	node_declare_statement->value = std::move(r_value);
	node_declare_statement->set_child_parents();
	node_declare_statement->parent = parent;
	return Result<std::unique_ptr<NodeSingleDeclaration>>(std::move(node_declare_statement));
}

Result<std::unique_ptr<NodeSingleAssignment>> Parser::parse_single_assign_statement(NodeAST* parent) {
	auto l_values = parse_l_values(parent);
	if(l_values.is_error()) {
		return Result<std::unique_ptr<NodeSingleAssignment>>(l_values.get_error());
	}
	auto values = std::move(l_values.unwrap());
    auto node_assign_statement_res = parse_assign_statement(std::move(values), parent);
    if(node_assign_statement_res.is_error())
        Result<std::unique_ptr<NodeSingleAssignment>>(node_assign_statement_res.get_error());
    const auto node_assign_statement = std::move(node_assign_statement_res.unwrap());
    if(node_assign_statement->l_values.size() != 1) {
        return Result<std::unique_ptr<NodeSingleAssignment>>(CompileError(ErrorType::ParseError,
                                                                          "Incorrect Syntax in <Single Assign Statement>.", peek().line, "One Assignment", std::to_string(node_assign_statement->l_values.size()), peek().file));
    }
    if(node_assign_statement->r_values->params.size() != 1) {
        return Result<std::unique_ptr<NodeSingleAssignment>>(CompileError(ErrorType::ParseError,
                                                                          "Incorrect Syntax in <Single Assign Statement>.", peek().line, "One Assignment", std::to_string(node_assign_statement->r_values->params.size()), peek().file));
    }
	auto ref = std::move(node_assign_statement->l_values[0]);
	auto tok = ref->tok;
    auto node_single_assign_statement = std::make_unique<NodeSingleAssignment>(
            std::move(ref), std::move(node_assign_statement->r_values->params[0]), tok);
    return Result<std::unique_ptr<NodeSingleAssignment>>(std::move(node_single_assign_statement));
}

Result<std::vector<std::unique_ptr<NodeReference>>> Parser::parse_l_values(NodeAST* parent) {
	// make it possible to have more than one variable before assign
	std::vector<std::unique_ptr<NodeReference>> vars;
	if(peek().type == token::COMMA) {
		auto error = CompileError(ErrorType::SyntaxError, "Found invalid <l_value> Syntax.", "", peek());
		error.m_message += " Lists of <l_values> must not start with a <comma>.";
		error.exit();
	}
	do {
		if(peek().type == token::COMMA) consume();
		// ui_control
		if (peek().type == token::KEYWORD) {
			auto ref = parse_reference_chain(parent);
			if (ref.is_error()) {
				return Result<std::vector<std::unique_ptr<NodeReference>>>(ref.get_error());
			}
			auto reference = std::move(ref.unwrap());
			vars.push_back(std::unique_ptr<NodeReference>(static_cast<NodeReference*>(reference.release())));
		} else {
			return Result<std::vector<std::unique_ptr<NodeReference>>>(CompileError(ErrorType::ParseError,
																		 "Found invalid <l_value> Syntax.", "<keyword>", peek()));
		}
	} while(peek().type == token::COMMA);
	if(vars.empty()) {
		auto error = CompileError(ErrorType::ParseError, "Found invalid <l_value> Syntax.", "", peek());
		error.m_message += " At least one <l_value> is required.";
		error.exit();
	}
	return Result<std::vector<std::unique_ptr<NodeReference>>>(std::move(vars));
}

bool Parser::is_func_call_reference_chain(NodeReference& ref) {
	auto chain = ref.cast<NodeAccessChain>();
	if (chain && chain->chain.back()->cast<NodeFunctionCall>()) {
		return true;
	}
	return false;
}

Result<std::unique_ptr<NodeAssignment>> Parser::parse_assign_statement(std::vector<std::unique_ptr<NodeReference>> l_values, NodeAST* parent) {
    auto node_assign_statement = std::make_unique<NodeAssignment>(l_values[0]->tok);

    if(peek().type != token::ASSIGN) {
        return Result<std::unique_ptr<NodeAssignment>>(CompileError(ErrorType::SyntaxError,
                                                                    "Found invalid Assign Statement Syntax.", ":=", peek()));
    }
    consume(); // consume :=

	std::unique_ptr<NodeParamList> assignees = nullptr;
    auto assignee =  parse_multiple_values(node_assign_statement.get()); //_parse_assignee();
    if(assignee.is_error()) {
        return Result<std::unique_ptr<NodeAssignment>>(assignee.get_error());
    }
	assignees = std::move(assignee.unwrap());
    node_assign_statement->l_values = std::move(l_values);
    node_assign_statement->r_values = std::move(assignees);
    node_assign_statement->set_child_parents();
    node_assign_statement->parent = parent;
    return Result<std::unique_ptr<NodeAssignment>>(std::move(node_assign_statement));
}

Result<std::unique_ptr<NodeCompoundAssignment>> Parser::parse_compound_assign_statement(
	std::unique_ptr<NodeReference> l_value,
	NodeAST *parent) {
	auto error = CompileError(ErrorType::SyntaxError, "", "", peek());

	auto node_compound_assign_statement = std::make_unique<NodeCompoundAssignment>(get_tok());
	auto valid_operator_tokens = extract_tokens_from_map(ALL_OPERATORS);
	if (!valid_operator_tokens.contains(peek().type)) {
		error.set_message( "Invalid Operator Syntax for <Compound Assignment>.");
		std::vector<std::string> valid_tokens{};
		valid_tokens.reserve(valid_operator_tokens.size());
		for (auto& tok : valid_operator_tokens) {
			valid_tokens.push_back(std::string("<") + tokenStrings[static_cast<int>(tok)] + ">");
		}
		error.m_expected = StringUtils::join(valid_tokens, ',');
		return Result<std::unique_ptr<NodeCompoundAssignment>>(error);
	}
	auto op = consume(); // consume operator token
	if (peek().type != token::EQUAL) {
		error.set_message( "Invalid Operator Syntax for <Compound Assignment>. Expected <equal> after operator.");
		error.m_expected = "=";
		return Result<std::unique_ptr<NodeCompoundAssignment>>(error);
	}
	consume(); // consume equal token
	auto r_value = parse_binary_expr(node_compound_assign_statement.get());
	if (r_value.is_error()) {
		return Result<std::unique_ptr<NodeCompoundAssignment>>(r_value.get_error());
	}
	node_compound_assign_statement->l_value = std::move(l_value);
	node_compound_assign_statement->r_value = std::move(r_value.unwrap());
	node_compound_assign_statement->op = op.type;
	node_compound_assign_statement->set_child_parents();
	node_compound_assign_statement->parent = parent;
	return Result<std::unique_ptr<NodeCompoundAssignment>>(std::move(node_compound_assign_statement));
}

Result<std::unique_ptr<NodeReturn>> Parser::parse_return_statement(NodeAST* parent) {
	auto ret_tok = consume(); // consume return keyword
	auto error = CompileError(ErrorType::SyntaxError, "", "", ret_tok);
	auto node_return_statement = std::make_unique<NodeReturn>(get_tok());
	if(peek().type == token::ASSIGN) {
		error.set_message( "The <return> keyword is reserved for <Return> Statement within function definitions. Consider using a different name.");
		return Result<std::unique_ptr<NodeReturn>>(error);
	}
	auto return_params = parse_multiple_values(node_return_statement.get());
	if(return_params.is_error()) {
		return Result<std::unique_ptr<NodeReturn>>(return_params.get_error());
	}
	node_return_statement->return_variables = std::move(return_params.unwrap()->params);
	if(!m_current_function_def) {
		error.set_message( "Found <Return> Statement outside of function definition.");
		return Result<std::unique_ptr<NodeReturn>>(error);
	}
	m_current_function_def->num_return_params = node_return_statement->return_variables.size();
	node_return_statement->definition = m_current_function_def;
    node_return_statement->set_child_parents();
	node_return_statement->parent = parent;
	return Result<std::unique_ptr<NodeReturn>>(std::move(node_return_statement));
}

Result<std::unique_ptr<NodeDelete>> Parser::parse_delete_statement(NodeAST* parent) {
	consume(); // consume delete keyword
	auto node_delete_stmt = std::make_unique<NodeDelete>(get_tok());
	while(peek().type != token::LINEBRK) {
		if(peek().type != token::KEYWORD) {
			auto error = CompileError(ErrorType::SyntaxError,"Found invalid <Delete> Syntax.", "", peek());
			error.m_message += " Only References to Pointers can be deleted.";
			return Result<std::unique_ptr<NodeDelete>>(error);
		}
		auto result = parse_reference_chain(node_delete_stmt.get());
		if(result.is_error()) {
			return Result<std::unique_ptr<NodeDelete>>(result.get_error());
		}
		auto ptr_result = std::move(result.unwrap());
		if(ptr_result->get_node_type() == NodeType::FunctionCall) {
			auto error = CompileError(ErrorType::SyntaxError,"Found invalid <Delete> Syntax.", "", peek());
			error.m_message += " <Function Calls> cannot be l_values in Delete Statements.";
			return Result<std::unique_ptr<NodeDelete>>(error);
		}
		node_delete_stmt->add_pointer(unique_ptr_cast<NodeReference>(std::move(ptr_result)));
		if(peek().type == token::COMMA) consume();
	}
	node_delete_stmt->set_child_parents();
	node_delete_stmt->parent = parent;
	return Result<std::unique_ptr<NodeDelete>>(std::move(node_delete_stmt));
}

Result<std::unique_ptr<NodeBreak>> Parser::parse_break_statement(NodeAST* parent) {
	consume(); // consume break keyword
	auto node_break_stmt = std::make_unique<NodeBreak>(get_tok());
	node_break_stmt->set_child_parents();
	node_break_stmt->parent = parent;
	return Result<std::unique_ptr<NodeBreak>>(std::move(node_break_stmt));
}

Result<std::unique_ptr<NodeStatement>> Parser::parse_statement(NodeAST* parent) {
    auto node_statement = std::make_unique<NodeStatement>(get_tok());
    _skip_linebreaks();
    std::unique_ptr<NodeAST> stmt;
    // assign statement
    if (peek().type == token::KEYWORD || peek().type == token::DECLARE
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
        	auto l_values = parse_l_values(node_statement.get());
        	if (l_values.is_error()) return Result<std::unique_ptr<NodeStatement>>(l_values.get_error());
        	// check if l_value is reference chain with function call at the end -> can stand isolated
        	auto values = std::move(l_values.unwrap());
        	if (values.size() == 1 and is_func_call_reference_chain(*values[0])) {
        		stmt = std::move(values[0]);
        	} else if (values.size() == 1 and peek().type != token::ASSIGN and peek().type != token::LINEBRK) {
        		auto compound_assign = parse_compound_assign_statement(std::move(values[0]), node_statement.get());
        		if (compound_assign.is_error()) {
        			return Result<std::unique_ptr<NodeStatement>>(compound_assign.get_error());
        		}
        		stmt = std::move(compound_assign.unwrap());
        	} else {
	            auto assign_stmt = parse_assign_statement(std::move(values), node_statement.get());
	            if (assign_stmt.is_error()) {
	                return Result<std::unique_ptr<NodeStatement>>(assign_stmt.get_error());
	            }
	            stmt = std::move(assign_stmt.unwrap());
        	}
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
        if(is_for_each_syntax()) {
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
	} else if (peek().type == token::RETURN) {
		auto return_stmt = parse_return_statement(node_statement.get());
		if (return_stmt.is_error()) {
			return Result<std::unique_ptr<NodeStatement>>(return_stmt.get_error());
		}
		stmt = std::move(return_stmt.unwrap());
	} else if(peek().type == token::BREAK) {
		auto break_stmt = parse_break_statement(node_statement.get());
		if (break_stmt.is_error()) {
			return Result<std::unique_ptr<NodeStatement>>(break_stmt.get_error());
		}
		stmt = std::move(break_stmt.unwrap());
	} else if (peek().type == token::DELETE) {
		auto delete_stmt = parse_delete_statement(node_statement.get());
		if (delete_stmt.is_error()) {
			return Result<std::unique_ptr<NodeStatement>>(delete_stmt.get_error());
		}
		stmt = std::move(delete_stmt.unwrap());
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
    auto node_body = std::make_unique<NodeBlock>(get_tok());
	auto start_token = consume(); // consume BEGIN_CALLBACK
    std::string begin_callback = start_token.val;
	if(begin_callback == "on init") {
		m_init_callback_idx = m_callbacks.size();
	}
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
	auto end_token = consume();
    std::string end_callback = end_token.val; // Consume END_CALLBACK
	node_callback->set_range(start_token, end_token);
    node_callback->begin_callback = std::move(begin_callback);
    node_callback->callback_id = std::move(callback_id);
    node_callback->statements = std::move(node_body);
    node_callback->end_callback = std::move(end_callback);
    node_callback->set_child_parents();
    node_callback->parent = parent;
    return Result<std::unique_ptr<NodeCallback>>(std::move(node_callback));
}

Result<std::unique_ptr<NodeNamespace>> Parser::parse_namespace(NodeAST *parent) {
	Token start_token = consume(); //consume namespace
	token end_construct = token::END_NAMESPACE;
	auto error = CompileError(ErrorType::SyntaxError, "Found invalid <namespace> syntax.", "", peek());
	if(peek().type != token::KEYWORD) {
		error.set_message( "Expected <namespace> name after <namespace> keyword.");
		return Result<std::unique_ptr<NodeNamespace>>(error);
	}
	auto name = consume(); //consume name
	auto l = consume_linebreak("<namespace>");
	if(l.is_error())
		return Result<std::unique_ptr<NodeNamespace>>(l.get_error());
	auto node_declarations = std::make_unique<NodeBlock>(start_token);
	std::vector<std::shared_ptr<NodeFunctionDefinition>> node_functions;
	while(peek().type != end_construct) {
		_skip_linebreaks();
		if(peek().type == token::DECLARE) {
			auto declare_stmt = parse_declare_statement(node_declarations.get());
			if(declare_stmt.is_error()) {
				return Result<std::unique_ptr<NodeNamespace>>(declare_stmt.get_error());
			}
			node_declarations->add_as_stmt(std::move(declare_stmt.unwrap()));
		} else if (peek().type == token::FUNCTION) {
			auto func = parse_function_definition(node_declarations.get());
			if(func.is_error()) {
				return Result<std::unique_ptr<NodeNamespace>>(func.get_error());
			}
			node_functions.push_back(std::move(func.unwrap()));
		} else if (peek().type == token::NAMESPACE) {
			auto namespace_def = parse_namespace(node_declarations.get());
			if (namespace_def.is_error()) {
				return Result<std::unique_ptr<NodeNamespace>>(namespace_def.get_error());
			}
			node_declarations->add_as_stmt(std::move(namespace_def.unwrap()));
		} else {
			error.add_message("<namespaces> can only contain <declare> statements or <function> definitions that get added to the global scope.");
			error.set_token(peek());
			return Result<std::unique_ptr<NodeNamespace>>(error);
		}
		_skip_linebreaks();
	}
	auto end_token = consume(); // consume end struct
	auto node_namespace = std::make_unique<NodeNamespace>(
		name.val,
		std::move(node_declarations),
		std::move(node_functions),
		name);
	node_namespace->set_range(start_token, end_token);
	node_namespace -> parent = parent;
	return Result<std::unique_ptr<NodeNamespace>>(std::move(node_namespace));
}

Result<std::unique_ptr<NodeProgram>> Parser::parse_program() {
    auto node_program = std::make_unique<NodeProgram>(get_tok());
	// int init_callback_idx = 0;
    while (peek().type != token::END_TOKEN) {
        _skip_linebreaks();
        if (peek().type == token::BEGIN_CALLBACK) {
            auto callback = parse_callback(node_program.get());
            if (callback.is_error())
                return Result<std::unique_ptr<NodeProgram>>(callback.get_error());
			m_callbacks.push_back(std::move(callback.unwrap()));
        } else if (peek().type == token::FUNCTION) {
			auto function = parse_function_definition(node_program.get());
			if (function.is_error())
				return Result<std::unique_ptr<NodeProgram>>(function.get_error());
			auto node_function = std::move(function.unwrap());
        	if (auto func = node_program->look_up_exact({node_function->header->name, (int)node_function->header->params.size()}, node_function->header->ty)) {
        		// was already declared, see if it overrides
        		if (node_function->override) {
        			NodeProgram::replace_function_definition(func, node_function);
        		} else if (func->override and !node_function->override) {
        			// function in map is already override and encountered function is not
        			// pass
        		} else if (func->override and node_function->override) {
        			auto error = CompileError(ErrorType::SyntaxError,"", "", node_function->header->tok);
        			error.set_message( "Found duplicate function definition with the same name and parameter count. Both have been marked"
							 "as <override>. The compiler will use the last encountered definition that has been marked as <override>.\n"
        					 "Consider removing the <override> keyword from one of the definitions.");
        			error.print();
        		} else {
        			auto error = CompileError(ErrorType::SyntaxError,"", "", node_function->header->tok);
        			error.set_message( "A function with this name and parameter count already exists. \n"
							 "To override it, use the <override> keyword. \n"
							 "To overload it, use <Union> types instead to define function templates accepting multiple types.");
        			return Result<std::unique_ptr<NodeProgram>>(error);
        		}
        	} else {
        		node_program->add_function_definition(node_function);
        	}
		} else if(peek().type == token::STRUCT) {
			auto struct_def = parse_struct(node_program.get());
			if(struct_def.is_error())
				return Result<std::unique_ptr<NodeProgram>>(struct_def.get_error());
			node_program->struct_definitions.push_back(std::move(struct_def.unwrap()));
		} else if (peek().type == token::CONST) {
			auto const_def = parse_const_statement(node_program->global_declarations.get());
			if (const_def.is_error()) {
				return Result<std::unique_ptr<NodeProgram>>(const_def.get_error());
			}
			node_program->global_declarations->add_as_stmt(std::move(const_def.unwrap()));
		} else if (peek().type == token::NAMESPACE) {
			auto namespace_def = parse_namespace(node_program.get());
			if (namespace_def.is_error()) {
				return Result<std::unique_ptr<NodeProgram>>(namespace_def.get_error());
			}
			node_program->namespaces.push_back(std::move(namespace_def.unwrap()));
		} else if (peek().type == token::DECLARE) {
			auto declare_stmt = parse_declare_statement(node_program->global_declarations.get());
			if (declare_stmt.is_error()) {
				return Result<std::unique_ptr<NodeProgram>>(declare_stmt.get_error());
			}
			node_program->global_declarations->add_as_stmt(std::move(declare_stmt.unwrap()));
		} else if (peek().type == token::FAMILY) {
			auto family_stmt = parse_family_statement(node_program->global_declarations.get());
			if (family_stmt.is_error()) {
				return Result<std::unique_ptr<NodeProgram>>(family_stmt.get_error());
			}
			node_program->global_declarations->add_as_stmt(std::move(family_stmt.unwrap()));
		} else if (peek().type == token::LIST) {
			auto list_block = parse_list_block(node_program->global_declarations.get());
			if (list_block.is_error()) {
				return Result<std::unique_ptr<NodeProgram>>(list_block.get_error());
			}
			node_program->global_declarations->add_as_stmt(std::move(list_block.unwrap()));
        } else {
            return Result<std::unique_ptr<NodeProgram>>(CompileError(ErrorType::ParseError,
             "Found unknown construct.", "<callback>, <function_definition>", peek()));
        }
        auto l = consume_linebreak("<program-level construct>");
        if(l.is_error())
            return Result<std::unique_ptr<NodeProgram>>(l.get_error());
        _skip_linebreaks();
    }
    node_program->callbacks = std::move(m_callbacks);
	if(m_init_callback_idx != -1) {
		node_program->init_callback = node_program->callbacks[m_init_callback_idx].get();
	} else {
		// no on init callback was declared
		auto init_tok = node_program->tok;
		auto init_callback = std::make_unique<NodeCallback>("on init", std::make_unique<NodeBlock>(init_tok), "end on", init_tok);
		node_program->callbacks.push_back(std::move(init_callback));
		node_program->init_callback = node_program->callbacks.back().get();
	}
	node_program->merge_function_definitions();
	// node_program->update_function_lookup();
	node_program->update_struct_lookup();
	node_program->check_unique_callbacks();
	node_program->init_callback = node_program->move_on_init_callback();
    return Result<std::unique_ptr<NodeProgram>>(std::move(node_program));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_multiple_values(NodeAST* parent) {
	auto mult_values = std::make_unique<NodeParamList>(get_tok());
	while (true) {
		size_t backup_pos = m_pos; // backup token index
		if (auto exprResult = parse_expression(parent); !exprResult.is_error()) {
			mult_values->add_param(std::move(exprResult.unwrap()));
		} else {
			m_pos = backup_pos;
			auto param_list = parse_param_list(parent);
			if (param_list.is_error()) {
				return Result<std::unique_ptr<NodeParamList>>(param_list.get_error());
			}
			mult_values->add_param(std::move(param_list.unwrap()));
		}
		if (peek().type != token::COMMA) break;
		consume(); // consume comma
	}
	mult_values->parent = parent;
	return Result<std::unique_ptr<NodeParamList>>(std::move(mult_values));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_param_list(NodeAST* parent) {
    // auto param_list = std::make_unique<NodeParamList>(get_tok());
    // param_list->parent = parent;
    // auto result = _parse_into_param_list(param_list->params, param_list.get());
    // if (result.is_error()) {
    //     return Result<std::unique_ptr<NodeParamList>>(result.get_error());
    // }
    // return Result<std::unique_ptr<NodeParamList>>(std::move(param_list));

	auto param_list = std::make_unique<NodeParamList>(get_tok());
	if (peek().type == token::OPEN_PARENTH) consume(); // consume (
	while (true) {
		if (auto exprResult = parse_expression(parent); !exprResult.is_error()) {
			param_list->add_param(std::move(exprResult.unwrap()));
		} else {
			auto error = exprResult.get_error();
            error.m_message += " Found possible nested <ParameterList> Syntax. To denote <Array> initializers inside <ParameterLists>, use '[' and ']'.";
			return Result<std::unique_ptr<NodeParamList>>(error);
		}
		if (peek().type != token::COMMA) break;
		consume(); // consume comma
	}
	if (peek().type == token::CLOSED_PARENTH) consume(); // consume )

	param_list->parent = parent;
	return Result<std::unique_ptr<NodeParamList>>(std::move(param_list));
}

// Result<SuccessTag> Parser::_parse_into_param_list(std::vector<std::unique_ptr<NodeAST>>& params, NodeAST* parent) {
//     size_t end_backup_pos; // in case of declaration list and ui_slider sliddd(0,1), <-
//     while (true) {
// //		_skip_linebreaks();
//         if (peek().type == token::OPEN_PARENTH) {
//             size_t backup_pos = m_pos; // backup token index
//             if (auto exprResult = parse_expression(parent); !exprResult.is_error()) {
// 				// if(peek(-1).type == token::CLOSED_PARENTH) {
// 				// 	auto expr_result = std::move(exprResult.unwrap());
// 				// 	params.push_back(std::make_unique<NodeParamList>(expr_result->tok, std::move(expr_result)));
// 				// 	params.back()->parent = parent;
// 				// } else {
//                 	params.push_back(std::move(exprResult.unwrap()));
// 				// }
//             } else {
//                 m_pos = backup_pos; // set back token index
//                 consume(); // consume (
//                 auto nested_param_list = std::make_unique<NodeParamList>(get_tok());
//                 nested_param_list->parent = parent;
//                 auto nestedResult = _parse_into_param_list(nested_param_list->params, nested_param_list.get());
//                 if (nestedResult.is_error()) {
//                     return nestedResult;
//                 }
//                 params.push_back(std::move(nested_param_list));
//                 if (peek().type == token::CLOSED_PARENTH) consume(); // consume )
//             	// auto error = exprResult.get_error();
//             	// error.m_message += " Found nested <ParameterList> Syntax. To denote <Array> initializers inside <ParameterList>, use '[' and ']'.";
//             	// return Result<SuccessTag>(error);
//             }
//         } else {
//             auto exprResult = parse_expression(parent);
//             if (!exprResult.is_error()) {
//                 params.push_back(std::move(exprResult.unwrap()));
//             } else {
//                 // eg case declare ui_slider sli_bum(0,100), ui_button btn_buuuuu
//                 if(peek(end_backup_pos-m_pos).type == token::COMMA) {
//                     m_pos = end_backup_pos;
//                     break;
//                 }
//                 return Result<SuccessTag>(exprResult.get_error());
//             }
//         }
// //		_skip_linebreaks();
//         if (peek().type != token::COMMA) break;
//         end_backup_pos = m_pos;
//         consume(); // consume comma
// //		_skip_linebreaks();
//     }
//     return Result<SuccessTag>(SuccessTag{});
// }

Result<std::unique_ptr<NodeAST>> Parser::parse_init_list(NodeAST* parent) {
	consume(); // consume [
	auto init_list = std::make_unique<NodeInitializerList>(get_tok());
	while (peek().type != token::CLOSED_BRACKET) {
		if (auto exprResult = parse_expression(parent); !exprResult.is_error()) {
			init_list->add_element(std::move(exprResult.unwrap()));
		} else {
			return Result<std::unique_ptr<NodeAST>>(exprResult.get_error());
		}
		if (peek().type == token::COMMA) consume();
	}
	consume(); // consume ]
	init_list->parent = parent;
	return Result<std::unique_ptr<NodeAST>>(std::move(init_list));
}

Result<std::unique_ptr<NodeFunctionHeader>> Parser::parse_function_header(NodeAST* parent) {
    std::string func_name;
	Token start_token = consume();
	Token end_token = start_token;
    func_name = start_token.val;
    auto node_function_header = std::make_unique<NodeFunctionHeader>(func_name, start_token);
	if (peek().type == token::OPEN_PARENTH) {
		consume(); // consume (
		if (peek().type != token::CLOSED_PARENTH) {
			while (peek().type != token::CLOSED_PARENTH) {
				if (peek().type != token::KEYWORD and peek().type != token::REF) {
					auto error = CompileError(ErrorType::SyntaxError, "Found invalid <FunctionHeader> Syntax.", "", peek());
					error.m_message += " Only valid <Variables> can be parameters in a function definition.";
					error.exit();
				}
				auto result = parse_function_param(node_function_header.get());
				if (result.is_error()) {
					return Result<std::unique_ptr<NodeFunctionHeader>>(result.get_error());
				}
				node_function_header->add_param(std::move(result.unwrap()));
				if (peek().type == token::COMMA) consume();
			}
		}
		if (peek().type == token::CLOSED_PARENTH) {
			end_token = consume();
		}
	}

	// parse function type if definition
	Type* ty = TypeRegistry::Unknown;

	auto type = parse_type_annotation();
	if(type.is_error()) {
		return Result<std::unique_ptr<NodeFunctionHeader>>(type.get_error());
	}
	auto return_type = type.unwrap();
	if(return_type->get_type_kind() == TypeKind::Function) {
		auto error = CompileError(ErrorType::ParseError,"", "", peek());
		error.set_message( "Function type not allowed as return type.");
		error.exit();
	}

	node_function_header->create_function_type(return_type);
	node_function_header->set_range(start_token, end_token);
    node_function_header->parent = parent;
    return Result<std::unique_ptr<NodeFunctionHeader>>(std::move(node_function_header));
}

Result<std::unique_ptr<NodeFunctionHeaderRef>> Parser::parse_function_header_ref(NodeAST* parent) {
	std::string func_name;
	Token start_token = consume();
	func_name = start_token.val;
	auto node_function_header_ref = std::make_unique<NodeFunctionHeaderRef>(func_name, start_token);
	auto func_args = parse_function_args(node_function_header_ref.get());
	if(func_args.is_error()) {
		return Result<std::unique_ptr<NodeFunctionHeaderRef>>(func_args.get_error());
	}
	// parse function type if definition
	Type* ty = TypeRegistry::Unknown;
	node_function_header_ref->ty = ty;
	node_function_header_ref->args = std::move(func_args.unwrap());
	node_function_header_ref->set_child_parents();
	node_function_header_ref->parent = parent;
	// node_function_header_ref->create_function_type(TypeRegistry::Unknown);
	return Result<std::unique_ptr<NodeFunctionHeaderRef>>(std::move(node_function_header_ref));
}

Result<std::unique_ptr<NodeParamList>> Parser::parse_function_args(NodeAST* parent) {
    auto error = CompileError(ErrorType::ParseError,"", "", peek());
    auto func_args = std::make_unique<NodeParamList>(get_tok());
    if (peek().type == token::OPEN_PARENTH) {
    	if (peek(1).type == token::CLOSED_PARENTH) {
    		consume(); // consume (
    		consume(); // consume )
    	} else {
			auto param_list = parse_param_list(parent);
			if (param_list.is_error()) {
				return Result<std::unique_ptr<NodeParamList>>(param_list.get_error());
			}
			func_args = std::move(param_list.unwrap());
			func_args->set_child_parents();
    	}
    }
    return Result<std::unique_ptr<NodeParamList>>(std::move(func_args));
}


Result<std::unique_ptr<NodeFunctionCall>> Parser::parse_function_call(NodeAST* parent) {
    bool is_call = false;
	bool is_new = false;
    if(peek().type == token::CALL) {
        is_call = true;
        consume(); // consume call
    } else if (peek().type == token::NEW) {
		is_new = true;
		consume(); // consume new
		if(peek().type != token::KEYWORD) {
			auto error = CompileError(ErrorType::ParseError,"", "", peek());
			error.set_message( "Incorrect syntax for instantiating <Object>.");
			error.m_expected = "new <Object>";
			error.exit();
		}
	}
    auto node_function_call = std::make_unique<NodeFunctionCall>(get_tok());
    auto func_stmt = parse_function_header_ref(node_function_call.get());
    if(func_stmt.is_error()){
        return Result<std::unique_ptr<NodeFunctionCall>>(func_stmt.get_error());
    }
    node_function_call->is_call = is_call;
	node_function_call->is_new = is_new;
    node_function_call->function = std::move(func_stmt.unwrap());
    node_function_call->set_child_parents();
    node_function_call->parent = parent;
    return Result<std::unique_ptr<NodeFunctionCall>>(std::move(node_function_call));
}


Result<std::shared_ptr<NodeFunctionDefinition>> Parser::parse_function_definition(NodeAST* parent) {
    auto error = CompileError(ErrorType::ParseError,"", "", peek());
    auto start_token = consume(); //consume "function"
    auto node_function_definition = std::make_shared<NodeFunctionDefinition>(get_tok());
	m_current_function_def = node_function_definition;
    std::unique_ptr<NodeFunctionHeader> func_header;
    std::optional<std::unique_ptr<NodeDataStructure>> func_return_var;
    auto func_body = std::make_unique<NodeBlock>(get_tok(), true);
    bool func_override = false;
    if (peek().type != token::KEYWORD) {
        error.set_message( "Missing function name.");
        error.m_expected = "keyword";
        return Result<std::shared_ptr<NodeFunctionDefinition>>(error);
    }
    auto header = parse_function_header(node_function_definition.get());
    if (header.is_error()) {
        return Result<std::shared_ptr<NodeFunctionDefinition>>(header.get_error());
    }
    func_header = std::move(header.unwrap());
    func_return_var = {};
    if (peek().type == token::ARROW) {
        consume();
        if(peek().type == token::RETURN) {
            m_tokens[m_pos].type = token::KEYWORD;
        }
        if (peek().type == token::KEYWORD) {
            auto return_vars = parse_declare_statement(node_function_definition.get());
            if (return_vars.is_error()) {
                Result<std::shared_ptr<NodeFunctionDefinition>>(return_vars.get_error());
            }
            auto return_var = std::move(return_vars.unwrap()->variable);
			if(return_var.size() > 1) {
				error.set_message( "Only on return variable allowed. Use <Return> Statement to return multiple values.");
				error.exit();
			}
			m_current_function_def->num_return_params = 1;
            func_return_var = std::move(return_var[0]);
        } else {
            error.set_message( "Missing return variable after ->");
            error.m_expected = "<return variable>";
            return Result<std::shared_ptr<NodeFunctionDefinition>>(error);
        }
    }
    if (peek().type == token::OVERRIDE) {
        consume();
        func_override = true;
    }
    if (peek().type != token::LINEBRK) {
        error.set_message( "Missing linebreak after function header.");
    	error.set_expected("linebreak");
        return Result<std::shared_ptr<NodeFunctionDefinition>>(error);
    }
    consume(); // consume linebreak

    while (peek().type != token::END_FUNCTION) {
        _skip_linebreaks();
        if(peek().type == token::END_FUNCTION) break;
        auto stmt = parse_statement(func_body.get());
        if (stmt.is_error()) {
            return Result<std::shared_ptr<NodeFunctionDefinition>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            func_body->add_stmt(std::move(stmt.unwrap()));
    }
    auto end_token = consume();
	node_function_definition->set_range(start_token, end_token);
    node_function_definition->header = std::move(func_header);
    node_function_definition->return_variable = std::move(func_return_var);
    node_function_definition->override = func_override;
    node_function_definition->body = std::move(func_body);
    node_function_definition->set_child_parents();
    node_function_definition->parent = parent;
	m_current_function_def = nullptr;
    return Result<std::shared_ptr<NodeFunctionDefinition>>(std::move(node_function_definition));
}

Result<std::unique_ptr<NodeDeclaration>> Parser::parse_declare_statement(NodeAST* parent) {
    auto node_declare_statement = std::make_unique<NodeDeclaration>(get_tok());
	auto start_token = get_tok();
    if(peek().type == token::DECLARE) start_token = consume(); //consume declare
    std::vector<std::unique_ptr<NodeDataStructure>> to_be_declared;
	if(!modifier_keywords.contains(peek().type) and peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeDeclaration>>(CompileError(ErrorType::ParseError,
																	 "Incorrect syntax in declare statement. Found unknown <modifier>.", "<ui_control>, <variable>, <array>", peek()));
	}
    do {
        if(peek().type == token::COMMA) consume();
        // ui_control
        if (peek().type == token::UI_CONTROL xor peek(1).type == token::UI_CONTROL) {
            auto parsed_ui_control = parse_declare_ui_control(node_declare_statement.get());
            if (parsed_ui_control.is_error()) {
                return Result<std::unique_ptr<NodeDeclaration>>(parsed_ui_control.get_error());
            }
            to_be_declared.push_back(std::move(parsed_ui_control.unwrap()));
            // array
        } else if (is_array_declaration()) {
            auto parsed_arr = parse_declare_array(node_declare_statement.get());
            if(parsed_arr.is_error()) {
                return Result<std::unique_ptr<NodeDeclaration>>(parsed_arr.get_error());
            }
            to_be_declared.push_back(std::move(parsed_arr.unwrap()));
            // variable
        } else if(is_variable_declaration()) {
            auto parsed_var = parse_declare_variable(node_declare_statement.get());
            if (parsed_var.is_error()) {
                return Result<std::unique_ptr<NodeDeclaration>>(parsed_var.get_error());
            }
            to_be_declared.push_back(std::move(parsed_var.unwrap()));
        } else {
            return Result<std::unique_ptr<NodeDeclaration>>(CompileError(ErrorType::ParseError,
                                                                         "Incorrect syntax in declare statement.", "<ui_control>, <variable>, <array>", peek()));
        }
    } while(peek().type == token::COMMA);

    std::unique_ptr<NodeParamList> assignees = nullptr;
    // if there is an assignment following
    if (peek().type == token::ASSIGN) {
        consume(); //consume :=
		// // check here if the following assignment starts with a parenthesis in case of array declaration
		// bool starts_with_parenth = peek().type == token::OPEN_PARENTH;
		// bool is_array = to_be_declared.at(0)->get_node_type() == NodeType::Array || to_be_declared.at(0)->get_node_type() == NodeType::NDArray;

        auto assignee = parse_multiple_values(node_declare_statement.get());
        if(assignee.is_error()) {
            return Result<std::unique_ptr<NodeDeclaration>>(assignee.get_error());
        }
        assignees = std::move(assignee.unwrap());
		// // if assignment starts with parenth and has only one member -> nested param list
		// if(starts_with_parenth and is_array and assignees->params.size() == 1) {
		// 	auto nested_param_list = std::make_unique<NodeParamList>(get_tok(), std::move(assignees));
		// 	assignees = std::move(nested_param_list);
		// }
    } else {
        // initializes empty param list
        assignees = std::make_unique<NodeParamList>(get_tok());
    }
	node_declare_statement->set_range(start_token, get_tok());
    node_declare_statement->variable = std::move(to_be_declared);
    node_declare_statement->value = std::move(assignees);
    node_declare_statement->set_child_parents();
    node_declare_statement->parent = parent;
    return Result<std::unique_ptr<NodeDeclaration>>(std::move(node_declare_statement));
}

int Parser::peek_past_modifiers() {
	int lookahead_index = 0;
	std::vector<token> seen_modifiers;
	bool has_persistence_keyword = false;

	// Gehe durch alle Modifikator-Tokens am Anfang
	while (modifier_keywords.contains(peek(lookahead_index).type)) {
		token current_type = peek(lookahead_index).type;

		// Verhindere Duplikate (z.B. "static static")
		if (std::ranges::find(seen_modifiers, current_type) != seen_modifiers.end()) {
			auto error = CompileError(ErrorType::SyntaxError, "Found duplicate modifier in variable declaration.", "", peek(lookahead_index));
			error.exit();
			return -1; // Fehler: Duplikat gefunden
		}
		seen_modifiers.push_back(current_type);
		lookahead_index++;
	}
	return lookahead_index;
}

bool Parser::is_variable_declaration() {
	const int final_index = peek_past_modifiers();
	// Bei einem Fehler in den Modifikatoren ist es keine gültige Deklaration.
	if (final_index < 0) {
		return false;
	}
	// Nach den Modifikatoren muss ein KEYWORD folgen.
	return peek(final_index).type == token::KEYWORD;
}

bool Parser::is_array_declaration() {
	const int final_index = peek_past_modifiers();
	if (final_index < 0) {
		return false;
	}
	return peek(final_index).type == token::KEYWORD &&
		   peek(final_index + 1).type == token::OPEN_BRACKET;
}

std::optional<Token> Parser::get_persistent_keyword(const Token& tok) {
    if(tok.type == token::READ || tok.type == token::PERS || tok.type == token::INSTPERS) {
        return tok;
    }
    return {};
}


Result<std::unique_ptr<NodeVariable>> Parser::parse_declare_variable(NodeAST* parent) {
    bool is_local = false;
    bool is_global = false;
    auto var_type = DataType::Mutable;
	NodeDataStructure::Kind kind = NodeDataStructure::Kind::None;
	bool has_persistence_keyword = false;
	std::optional<Token> persistence{};
	while (modifier_keywords.contains(peek().type)) {
		Token current_tok = peek();
		auto error = CompileError(ErrorType::SyntaxError, "Found invalid variable declaration syntax.", "", peek());
		switch (current_tok.type) {
			case token::READ:
			case token::PERS:
			case token::INSTPERS:
				if (has_persistence_keyword) {
					error.m_message += " Only one persistence keyword is allowed.";
					return Result<std::unique_ptr<NodeVariable>>(error);
				}
				has_persistence_keyword = true;
				persistence = current_tok;
				break;
			case token::LOCAL:
				if (is_global) {
					error.m_message += " Cannot be both <local> and <global>.";
					return Result<std::unique_ptr<NodeVariable>>(error);
				}
				is_local = true;
				kind = NodeDataStructure::Kind::Local;
				break;
			case token::GLOBAL:
				if (is_local) {
					error.m_message += " Cannot be both <global> and <local>.";
					return Result<std::unique_ptr<NodeVariable>>(error);
				}
				is_global = true;
				break;

			case token::CONST:
				if (var_type == DataType::Polyphonic) {
					error.m_message += " Cannot be both <const> and <polyphonic>.";
					return Result<std::unique_ptr<NodeVariable>>(error);
				}
				var_type = DataType::Const;
				break;
			case token::POLYPHONIC:
				if (var_type == DataType::Const) {
					error.m_message += " Cannot be both <const> and <polyphonic>.";
					return Result<std::unique_ptr<NodeVariable>>(error);
				}
				var_type = DataType::Polyphonic;
				break;
			case token::STATIC:
				if (var_type == DataType::Polyphonic) {
					error.m_message += " Cannot be both <static> and <polyphonic>.";
					return Result<std::unique_ptr<NodeVariable>>(error);
				}
				if (is_local) {
					error.m_message += " Cannot be both <local> and <static>.";
					return Result<std::unique_ptr<NodeVariable>>(error);
				}
				kind = NodeDataStructure::Kind::Static;
				break;
			default: break;
		}
		consume(); // Verarbeite den Modifikator-Token
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
	auto type = parse_type_annotation(node_variable->ty);
	if(type.is_error()) {
		return Result<std::unique_ptr<NodeVariable>>(type.get_error());
	}
    node_variable->is_local = is_local;
    node_variable->is_global = is_global;
	node_variable->kind = kind;
	node_variable->ty = type.unwrap();
    return Result<std::unique_ptr<NodeVariable>>(std::move(node_variable));
}

Result<std::unique_ptr<NodeDataStructure>> Parser::parse_declare_array(NodeAST* parent) {
	auto start_token = get_tok();
	bool is_local = false;
    bool is_global = false;
    auto var_type = DataType::Mutable;
	NodeDataStructure::Kind kind = NodeDataStructure::Kind::None;
	bool has_persistence_keyword = false;
	std::optional<Token> persistence{};
	auto error = CompileError(ErrorType::SyntaxError, "Found invalid array declaration syntax.", "", peek());
	while (modifier_keywords.contains(peek().type)) {
		Token current_tok = peek();
		switch (current_tok.type) {
			case token::READ:
			case token::PERS:
			case token::INSTPERS:
				if (has_persistence_keyword) {
					error.m_message += " Only one persistence keyword is allowed.";
					return Result<std::unique_ptr<NodeDataStructure>>(error);
				}
				has_persistence_keyword = true;
				persistence = current_tok;
				break;
			case token::LOCAL:
				if (is_global) {
					error.m_message += " Cannot be both <local> and <global>.";
					return Result<std::unique_ptr<NodeDataStructure>>(error);
				}
				is_local = true;
				kind = NodeDataStructure::Kind::Local;
				break;
			case token::GLOBAL:
				if (is_local) {
					error.m_message += " Cannot be both <global> and <local>.";
					return Result<std::unique_ptr<NodeDataStructure>>(error);
				}
				is_global = true;
				break;
			case token::CONST:
				var_type = DataType::Const;
				break;
			case token::POLYPHONIC:
				error.m_message += " Arrays cannot be <polyphonic>.";
				return Result<std::unique_ptr<NodeDataStructure>>(error);
				break;
			case token::STATIC:
				if (is_local) {
					error.m_message += " Cannot be both <local> and <static>.";
					return Result<std::unique_ptr<NodeDataStructure>>(error);
				}
				kind = NodeDataStructure::Kind::Static;
				break;
			default: break;
		}
		consume(); // Verarbeite den Modifikator-Token
	}
    if(peek().type != token::KEYWORD) {
    	error.m_expected = "array name (<keyword>)";
        return Result<std::unique_ptr<NodeDataStructure>>(error);
    }
    auto parsed_arr = parse_array(parent, persistence, var_type);
    if(parsed_arr.is_error()) {
        return Result<std::unique_ptr<NodeDataStructure>>(parsed_arr.get_error());
    }
    auto node_array = std::move(parsed_arr.unwrap());
	auto type = parse_type_annotation(node_array->ty);
	if(type.is_error()) {
		return Result<std::unique_ptr<NodeDataStructure>>(type.get_error());
	}
	auto end_token = peek(-1);
	node_array->set_range(start_token, end_token);
    node_array->is_local = is_local;
    node_array->is_global = is_global;
	node_array->ty = type.unwrap();
    return Result<std::unique_ptr<NodeDataStructure>>(std::move(node_array));
}

Result<std::unique_ptr<NodeUIControl>> Parser::parse_declare_ui_control(NodeAST* parent) {
    auto persistence = get_persistent_keyword(peek());
    if(persistence) {
        consume();
    }
    auto node_ui_control = std::make_unique<NodeUIControl>(get_tok());
    DataType var_type = DataType::UIControl;
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
        auto parsed_arr = parse_array(node_ui_control.get(), persistence, var_type);
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
            control_array_sizes = std::make_unique<NodeParamList>(get_tok());
        }
        control_var = std::move(array);
    } else {
        auto parsed_var = parse_variable(node_ui_control.get(), persistence, var_type);
        if(parsed_var.is_error()) {
            return Result<std::unique_ptr<NodeUIControl>>(parsed_var.get_error());
        }
        control_var = std::move(parsed_var.unwrap());
    }
	auto type = parse_type_annotation(control_var->ty);
	if(type.is_error()) {
		return Result<std::unique_ptr<NodeUIControl>>(type.get_error());
	}
	control_var->ty = type.unwrap();
    std::unique_ptr<NodeParamList> control_params;
    if(peek().type == token::OPEN_PARENTH) {
        auto param_list = parse_param_list(node_ui_control.get());
        if (param_list.is_error()) {
            Result<std::unique_ptr<NodeUIControl>>(param_list.get_error());
        }
        control_params = std::move(param_list.unwrap());
    } else {
        control_params = std::make_unique<NodeParamList>(get_tok());
    }
    node_ui_control->ui_control_type = std::move(ui_control_type);
    node_ui_control->control_var = std::move(control_var);
    node_ui_control->params = std::move(control_params);
    node_ui_control->sizes = std::move(control_array_sizes);
    node_ui_control->set_child_parents();
    return Result<std::unique_ptr<NodeUIControl>>(std::move(node_ui_control));
}

Result<std::unique_ptr<NodeIf>> Parser::parse_if_statement(NodeAST* parent) {
    auto node_if_statement = std::make_unique<NodeIf>(get_tok());
    //consume if
    auto start_token = consume();
	auto end_token = start_token;
    auto condition_result = parse_expression(node_if_statement.get());
    if(condition_result.is_error()) {
        return Result<std::unique_ptr<NodeIf>>(condition_result.get_error());
    }
    auto condition = std::move(condition_result.unwrap());

    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeIf>>(CompileError(ErrorType::SyntaxError,
                                                            "Expected linebreak after if-condition.", "linebreak", peek()));
    }
    consume(); // consume linebreak
    _skip_linebreaks();
    auto if_statements = std::make_unique<NodeBlock>(get_tok());
    while (peek().type != token::END_IF && peek().type != token::ELSE) {
        if(peek().type == token::END_IF or peek().type == token::ELSE) break;
        auto stmt = parse_statement(node_if_statement.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeIf>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            if_statements->statements.push_back(std::move(stmt.unwrap()));
    }
    bool no_end_if = false;
    auto else_statements = std::make_unique<NodeBlock>(get_tok());
    else_statements->parent = node_if_statement.get();
    if(peek().type == token::ELSE) {
        consume();
        if(not(peek().type == token::IF || peek().type == token::LINEBRK)) {
            return Result<std::unique_ptr<NodeIf>>(CompileError(ErrorType::SyntaxError,
                                                                "Expected linebreak after else-condition.", "linebreak", peek()));
        }
        if(peek().type == token::IF) {
            no_end_if = true;
            auto stmt = parse_if_statement(node_if_statement.get());
            if (stmt.is_error()) {
                return Result<std::unique_ptr<NodeIf>>(stmt.get_error());
            }
            auto stmt_val = std::make_unique<NodeStatement>(std::move(stmt.unwrap()), get_tok());
            else_statements->statements.push_back(std::move(stmt_val));
        } else {
            while (peek().type != token::END_IF) {
				_skip_linebreaks();
				if(peek().type == token::END_IF) break;
                auto stmt = parse_statement(node_if_statement.get());
                if (stmt.is_error()) {
                    return Result<std::unique_ptr<NodeIf>>(stmt.get_error());
                }
                if(stmt.unwrap()->statement)
                    else_statements->statements.push_back(std::move(stmt.unwrap()));
            }
        }
    }
    if(not(peek().type == token::END_IF || no_end_if)) {
        return Result<std::unique_ptr<NodeIf>>(CompileError(ErrorType::SyntaxError,
                                                            "Missing end token.", "end if", peek()));
    }
    if (!no_end_if) end_token = consume();
	node_if_statement->set_range(start_token, end_token);
    node_if_statement->condition = std::move(condition);
    node_if_statement->if_body = std::move(if_statements);
    node_if_statement->else_body = std::move(else_statements);
    node_if_statement->set_child_parents();
    node_if_statement->parent = parent;
    return Result<std::unique_ptr<NodeIf>>(std::move(node_if_statement));
}

Result<std::unique_ptr<NodeFor>> Parser::parse_for_statement(NodeAST* parent) {
    auto node_for_statement = std::make_unique<NodeFor>(get_tok());
    //consume for
    auto start_token = consume();
    auto assign_stmt = parse_single_assign_statement(node_for_statement.get());
    if(assign_stmt.is_error()) {
        return Result<std::unique_ptr<NodeFor>>(assign_stmt.get_error());
    }
    auto iterator = std::move(assign_stmt.unwrap());
    if(not(peek().type == token::TO || peek().type == token::DOWNTO)) {
        return Result<std::unique_ptr<NodeFor>>(CompileError(ErrorType::SyntaxError,
                                                             "Incorrect <for-loop> Syntax.", "to/downto", peek()));
    }
    token to = consume().type; //consume to or downto
    auto expression_stmt = parse_binary_expr(node_for_statement.get());
    if(expression_stmt.is_error()) {
        return Result<std::unique_ptr<NodeFor>>(expression_stmt.get_error());
    }
    auto iterator_end = std::move(expression_stmt.unwrap());
    std::unique_ptr<NodeAST> step;
    if(peek().type == token::STEP) {
        consume(); // consume step
        auto step_expression = parse_binary_expr(node_for_statement.get());
        if(step_expression.is_error()) {
            return Result<std::unique_ptr<NodeFor>>(step_expression.get_error());
        }
        step = std::move(step_expression.unwrap());
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeFor>>(CompileError(ErrorType::SyntaxError,
                                                             "Missing linebreak in <for-loop>", "linebreak", peek()));
    }
    consume(); //consume linebreak
    auto node_body = std::make_unique<NodeBlock>(get_tok());
    while (peek().type != token::END_FOR) {
        _skip_linebreaks();
        if(peek().type == token::END_FOR) break;
        auto stmt = parse_statement(node_body.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeFor>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            node_body->statements.push_back(std::move(stmt.unwrap()));
    }
    auto end_token = consume(); // consume end for
	node_for_statement->set_range(start_token, end_token);
    node_for_statement->iterator = std::move(iterator);
    node_for_statement->to = to;
    node_for_statement->iterator_end = std::move(iterator_end);
    node_for_statement->body = std::move(node_body);
    node_for_statement->step = std::move(step);
    node_for_statement->set_child_parents();
    node_for_statement->parent = parent;
    return Result<std::unique_ptr<NodeFor>>(std::move(node_for_statement));
}

bool Parser::is_for_each_syntax() {
    const size_t begin = m_pos;
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

Result<std::unique_ptr<NodeForEach>> Parser::parse_for_each_statement(NodeAST* parent) {
    auto node_for_statement = std::make_unique<NodeForEach>(get_tok());
    //consume for
    auto start_token = consume();

	// make it possible to have more than one variable before assign
	std::vector<std::unique_ptr<NodeVariable>> pair;
	if(peek().type == token::COMMA) {
		auto error = CompileError(ErrorType::SyntaxError, "Found invalid <For-each> Statement Syntax.", "", peek());
		error.m_message += " <key, value> pair references must not start with a <comma>.";
		error.exit();
	}
	do {
		if(peek().type == token::COMMA) consume();
		// ui_control
		if (peek().type == token::KEYWORD) {
			auto variable = parse_variable(node_for_statement.get());
			if (variable.is_error()) {
				return Result<std::unique_ptr<NodeForEach>>(variable.get_error());
			}
			auto var = std::move(variable.unwrap());
			var->is_local = true;
			pair.push_back(std::move(var));
		} else {
			return Result<std::unique_ptr<NodeForEach>>(CompileError(ErrorType::ParseError,
				"Incorrect syntax in <key, value> pair.", "<variable>", peek()));
		}
	} while(peek().type == token::COMMA);
	if(pair.size() > 2) {
		auto error = CompileError(ErrorType::SyntaxError, "Found invalid <For-each> Statement Syntax.", "", peek());
		error.m_message += " <key> or <key, value> pairs cannot have more than two variables.";
		error.exit();
	}
	std::unique_ptr<NodeFunctionParam> key;
	std::unique_ptr<NodeFunctionParam> value;
	if(pair.size() == 1) {
		value = std::make_unique<NodeFunctionParam>(std::move(pair[0]));
	}
	if(pair.size() == 2) {
		key = std::make_unique<NodeFunctionParam>(std::move(pair[0]));
		value = std::make_unique<NodeFunctionParam>(std::move(pair[1]));
	}

    if(peek().type != token::IN)
        return Result<std::unique_ptr<NodeForEach>>(CompileError(ErrorType::SyntaxError,
                                                                 "Incorrect Syntax for range-based <for-loop>.", "in", peek()));
    Token in = consume(); //consume in
	std::unique_ptr<NodeAST> node_range;
	if(peek().type == token::OPEN_PARENTH) {
		auto range = parse_param_list(node_for_statement.get());
		if(range.is_error()) {
			return Result<std::unique_ptr<NodeForEach>>(range.get_error());
		}
		node_range = std::move(range.unwrap());
	} else {
		auto range = parse_reference_chain(node_for_statement.get());
		if(range.is_error()) {
			return Result<std::unique_ptr<NodeForEach>>(range.get_error());
		}
		node_range = std::move(range.unwrap());
	}
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeForEach>>(CompileError(ErrorType::SyntaxError,
                                                                 "Missing linebreak in <for-loop>", "linebreak", peek()));
    }
    consume(); //consume linebreak
    auto node_body = std::make_unique<NodeBlock>(get_tok());
    while (peek().type != token::END_FOR) {
        _skip_linebreaks();
        if(peek().type == token::END_FOR) break;
        auto stmt = parse_statement(node_body.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeForEach>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            node_body->statements.push_back(std::move(stmt.unwrap()));
    }
    auto end_token = consume(); // consume end for
	node_for_statement->set_range(start_token, end_token);
    node_for_statement->key = std::move(key);
	node_for_statement->value = std::move(value);
    node_for_statement->range = std::move(node_range);
    node_for_statement->body = std::move(node_body);
    node_for_statement->set_child_parents();
    node_for_statement->parent = parent;
    return Result<std::unique_ptr<NodeForEach>>(std::move(node_for_statement));
}


Result<std::unique_ptr<NodeWhile>> Parser::parse_while_statement(NodeAST* parent) {
    auto node_while_statement = std::make_unique<NodeWhile>(get_tok());
    auto start_token = consume(); // consume while
    auto condition_result = parse_expression(node_while_statement.get());
    if(condition_result.is_error()) {
        return Result<std::unique_ptr<NodeWhile>>(condition_result.get_error());
    }
    auto condition = std::move(condition_result.unwrap());
    if(not(condition->ty == TypeRegistry::Boolean || condition->ty == TypeRegistry::Comparison)) {
        return Result<std::unique_ptr<NodeWhile>>(CompileError(ErrorType::SyntaxError,
                                                               "While Statement needs condition.", peek().line, "condition", condition->get_string(), peek().file));
    }
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeWhile>>(CompileError(ErrorType::ParseError,
                                                               "Expected linebreak after while-condition.", "linebreak", peek()));
    }
    consume(); //consume linebreak
    auto node_body = std::make_unique<NodeBlock>(get_tok());
    while (peek().type != token::END_WHILE) {
        auto stmt = parse_statement(node_body.get());
        if (stmt.is_error()) {
            return Result<std::unique_ptr<NodeWhile>>(stmt.get_error());
        }
        if(stmt.unwrap()->statement)
            node_body->statements.push_back(std::move(stmt.unwrap()));
    }
    auto end_token = consume(); // consume end while
	node_while_statement->set_range(start_token, end_token);
    node_while_statement->condition = std::move(condition);
    node_while_statement->body = std::move(node_body);
    node_while_statement->set_child_parents();
    node_while_statement ->parent = parent;
    return Result<std::unique_ptr<NodeWhile>>(std::move(node_while_statement));
}

Result<std::unique_ptr<NodeSelect>> Parser::parse_select_statement(NodeAST* parent) {
    auto node_select_statement = std::make_unique<NodeSelect>(get_tok());
    auto start_token = consume(); //consume select
    auto expression = parse_expression(node_select_statement.get());
    if(peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeSelect>>(CompileError(ErrorType::SyntaxError,
			"Expected linebreak after select-expression.", "linebreak", peek()));
    }
    consume(); //consume linebreak
    _skip_linebreaks();
    if(peek().type != token::CASE) {
        return Result<std::unique_ptr<NodeSelect>>(CompileError(ErrorType::SyntaxError,
		"Expected cases in select-expression.", "case <expression>", peek()));
    }
    std::vector<std::pair<std::vector<std::unique_ptr<NodeAST>>, std::unique_ptr<NodeBlock>>> cases;
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
                    return Result<std::unique_ptr<NodeSelect>>(cas_result.get_error());
                cas.push_back(std::move(cas_result.unwrap()));
                if (peek().type == token::TO) {
                    consume(); // consume to
                    auto cas2_result = parse_expression(node_select_statement.get());
                    if (cas2_result.is_error())
                        return Result<std::unique_ptr<NodeSelect>>(cas2_result.get_error());
                    cas.push_back(std::move(cas2_result.unwrap()));
                }
            }
            if(peek().type != token::LINEBRK) {
                return Result<std::unique_ptr<NodeSelect>>(CompileError(ErrorType::SyntaxError,
                                                                        "Expected linebreak after case.", "linebreak", peek()));
			}
			consume(); //consume linebreak
			_skip_linebreaks();
			auto stmts = std::make_unique<NodeBlock>(get_tok());
			while(peek().type != token::END_SELECT && peek().type != token::CASE) {
				auto stmt = parse_statement(node_select_statement.get());
				if (stmt.is_error()) {
					return Result<std::unique_ptr<NodeSelect>>(stmt.get_error());
				}
				if(stmt.unwrap()->statement) {
					stmt.unwrap()->parent = stmts.get();
					stmts->statements.push_back(std::move(stmt.unwrap()));
				}
			}
			cases.emplace_back(std::move(cas),std::move(stmts));
		}
	}
	auto end_token = consume(); // consume end select
	node_select_statement->set_range(start_token, end_token);
	node_select_statement->expression = std::move(expression.unwrap());
	node_select_statement->cases = std::move(cases);
	node_select_statement->set_child_parents();
	node_select_statement->parent = parent;
	return Result<std::unique_ptr<NodeSelect>>(std::move(node_select_statement));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_family_statement(NodeAST* parent) {
	auto start_token = consume(); //consume family
	token end_construct = token::END_FAMILY;
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
															 "Found unknown family syntax.", "valid prefix", peek()));
	}
	auto prefix = consume(); //consume prefix
	auto node_family_statement = std::make_unique<NodeFamily>(prefix);
	auto l = consume_linebreak("<family statement>");
	if(l.is_error())
		return Result<std::unique_ptr<NodeAST>>(l.get_error());
	auto node_block = std::make_unique<NodeBlock>(start_token);
	while(peek().type != token::END_FAMILY) {
		_skip_linebreaks();
		auto declare_stmt = parse_statement(nullptr);
		if(declare_stmt.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(declare_stmt.get_error());
		}
		declare_stmt.unwrap() -> parent = node_block.get();
		if(declare_stmt.unwrap()->statement)
			node_block->statements.push_back(std::move(declare_stmt.unwrap()));
	}
	auto end_token = consume(); // consume end family
	node_family_statement->set_range(start_token, end_token);
	node_family_statement->prefix = prefix.val;
	node_family_statement->members = std::move(node_block);
	node_family_statement->set_child_parents();
	node_family_statement -> parent = parent;
	return Result<std::unique_ptr<NodeAST>>(std::move(node_family_statement));
}

Result<std::unique_ptr<NodeStruct>> Parser::parse_struct(NodeAST* parent) {
	auto start_token = consume(); //consume struct
	token end_construct = token::END_STRUCT;
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeStruct>>(CompileError(ErrorType::SyntaxError,
															 "Found unknown <struct> syntax.", "valid <struct> name", peek()));
	}
	auto name = consume(); //consume name
	auto l = consume_linebreak("<struct>");
	if(l.is_error())
		return Result<std::unique_ptr<NodeStruct>>(l.get_error());
	auto node_member_block = std::make_unique<NodeBlock>(start_token);
	std::vector<std::shared_ptr<NodeFunctionDefinition>> node_methods;
	while(peek().type != end_construct) {
		_skip_linebreaks();
		if(peek().type == token::DECLARE) {
			auto declare_stmt = parse_declare_statement(node_member_block.get());
			if(declare_stmt.is_error()) {
				return Result<std::unique_ptr<NodeStruct>>(declare_stmt.get_error());
			}
			node_member_block->add_stmt(std::make_unique<NodeStatement>(std::move(declare_stmt.unwrap()), start_token));
		} else if (peek().type == token::FUNCTION) {
			auto func = parse_function_definition(node_member_block.get());
			if(func.is_error()) {
				return Result<std::unique_ptr<NodeStruct>>(func.get_error());
			}
			node_methods.push_back(std::move(func.unwrap()));
		} else {
			return Result<std::unique_ptr<NodeStruct>>(CompileError(ErrorType::SyntaxError,
							 "Found unknown <struct> syntax.", "valid <struct> member or method", peek()));
		}
		_skip_linebreaks();
	}
	auto end_token = consume(); // consume end struct
	auto node_struct = std::make_unique<NodeStruct>(
		name.val,
		std::move(node_member_block),
		std::move(node_methods),
		name);
	node_struct->set_range(start_token, end_token);
	node_struct -> parent = parent;
	node_struct->rebuild_method_table();
	node_struct->rebuild_lookup_sets();
	return Result<std::unique_ptr<NodeStruct>>(std::move(node_struct));
}

Result<std::unique_ptr<NodeAST>> Parser::parse_list_block(NodeAST* parent) {
	Token construct = consume(); //consume list
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
															 "Found unknown <list> syntax.", "valid <keyword>", peek()));
	}
	Token name_tok = consume(); // consume keyword
	auto node_list_block = std::make_unique<NodeList>(name_tok);
	std::string name = name_tok.val;
	auto ty = TypeRegistry::get_type_from_identifier(name[0]);
	if(ty != TypeRegistry::Unknown) {
		name = name.erase(0,1);
		if(auto comp_ty = ty->cast<CompositeType>()) {
			ty = TypeRegistry::add_composite_type(CompoundKind::List, comp_ty->get_element_type(), comp_ty->get_dimensions());
		}
	}

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
	auto type = parse_type_annotation(ty);
	if(type.is_error()) {
		return Result<std::unique_ptr<NodeAST>>(type.get_error());
	}
	if(peek().type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
															 "Expected linebreak.", "linebreak", peek()));
	}

	consume(); // consume linebreak
	std::vector<std::unique_ptr<NodeInitializerList>> stmts;
	int32_t size = 0;
	while(peek().type != token::END_LIST) {
		_skip_linebreaks();
		if(peek().type == token::END_LIST) break;
		auto param_list = parse_param_list(node_list_block.get());
		if(param_list.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(param_list.get_error());
		}
		size += static_cast<int32_t>(param_list.unwrap()->size());
		auto init_list = param_list.unwrap()->to_initializer_list();
		init_list->parent = node_list_block.get();
		stmts.push_back(std::move(init_list));
		auto l = consume_linebreak("<statement>");
		if(l.is_error())
			return Result<std::unique_ptr<NodeAST>>(l.get_error());
	}
	consume(); // consume end_list
	node_list_block->name = name;
	node_list_block->size = size;
	node_list_block->body = std::move(stmts);
//	node_list_block->parent = node_declaration.get();
	node_list_block->ty = type.unwrap();
	auto node_declaration = std::make_unique<NodeSingleDeclaration>(std::move(node_list_block), node_list_block->tok);
	node_declaration->parent = parent;
	return Result<std::unique_ptr<NodeAST>>(std::move(node_declaration));
}


Result<std::unique_ptr<NodeAST>> Parser::parse_const_statement(NodeAST* parent) {
	auto start_token = consume(); //consume family, struct, const
	token end_construct = token::END_CONST;
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
															 "Found unknown const syntax.", "valid prefix", peek()));
	}
	auto prefix = consume(); //consume prefix
	auto node_const_statement = std::make_unique<NodeConst>(prefix);

	if(peek().type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::SyntaxError,
															 "Expected linebreak.", "linebreak", peek()));
	}
	consume(); // consume linebreak

	auto node_body = std::make_unique<NodeBlock>(start_token);
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
	auto end_token = consume(); // consume end_const
	node_const_statement->set_range(start_token, end_token);
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


Result<std::unique_ptr<NodeGetControl>> Parser::parse_get_control_statement(std::unique_ptr<NodeAST> ui_id, NodeAST* parent) {
	auto start_token = ui_id->tok;
	if(peek().type != token::ARROW) {
		return Result<std::unique_ptr<NodeGetControl>>(CompileError(ErrorType::SyntaxError,
                                                                    "Wrong control statement syntax.", "->", peek()));
	}
	consume(); // consume ->
	if(peek().type == token::DEFAULT) {
		m_tokens[m_pos].type = token::KEYWORD;
	}
	if(peek().type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeGetControl>>(CompileError(ErrorType::SyntaxError,
                                                                    "Wrong control statement syntax.", "<control_parameter>", peek()));
	}
	auto end_token = consume(); // consume control parameter
	auto control_param = end_token.val;
	auto node_get_control_statement = std::make_unique<NodeGetControl>(std::move(ui_id), control_param, get_tok());
	node_get_control_statement->set_range(start_token, end_token);
	node_get_control_statement->set_child_parents();
	node_get_control_statement->parent = parent;
	return Result<std::unique_ptr<NodeGetControl>>(std::move(node_get_control_statement));
}






