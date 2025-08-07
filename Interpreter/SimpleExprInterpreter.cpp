
#include "SimpleExprInterpreter.h"
#include "../Parser/Parser.h"

Result<int> SimpleExprInterpreter::parse_and_evaluate(std::vector<std::unique_ptr<PreNodeAST>> n) {
    auto parsed_result = parse(std::move(n));
    if(parsed_result.is_error())
        return Result<int>(parsed_result.get_error());
    auto result = evaluate_int_expression(parsed_result.unwrap());
    return result;
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::parse(std::vector<std::unique_ptr<PreNodeAST>> n) {
    auto nodes = std::move(n);
    m_pos = 0;
	auto root_result = parse_binary_expr(nodes, nullptr);
	if (root_result.is_error())
		return Result<std::unique_ptr<PreNodeAST>>(root_result.get_error());
	return Result<std::unique_ptr<PreNodeAST>>(std::move(root_result.unwrap()));
}

Result<int> SimpleExprInterpreter::evaluate_int_expression(std::unique_ptr<PreNodeAST>& root) {
    std::string preprocessor_error = "Statement needs to be evaluated to a single int value before compilation.";
    if (auto int_node = root->cast<PreNodeInt>()) {
        return Result<int>(int_node->integer);
    } else if (auto unary_expr_node = root->cast<PreNodeUnaryExpr>()) {
        auto operandValueStmt = evaluate_int_expression(unary_expr_node->operand);
        if (operandValueStmt.is_error()) return Result<int>(operandValueStmt.get_error());
        int operandValue = operandValueStmt.unwrap();
        if (unary_expr_node->op == token::SUB) { // Assuming SUB represents the '-' unary operator
            return Result<int>(-operandValue);
        }
        // Add other unary operations here if needed
        return Result<int>(CompileError(ErrorType::PreprocessorError,
            "Unsupported unary operation. " + preprocessor_error,
            "-", unary_expr_node->tok));
    } else if (auto binary_expr_node = root->cast<PreNodeBinaryExpr>()) {
        auto left_value_stmt = evaluate_int_expression(binary_expr_node->left);
        if (left_value_stmt.is_error()) return Result<int>(left_value_stmt.get_error());
        int left_value = left_value_stmt.unwrap();
        auto right_value_stmt = evaluate_int_expression(binary_expr_node->right);
        if (right_value_stmt.is_error()) return Result<int>(right_value_stmt.get_error());
        int right_value = right_value_stmt.unwrap();
        if (binary_expr_node->op == token::ADD) {
            return Result<int>(left_value + right_value);
        } else if (binary_expr_node->op == token::SUB) {
            return Result<int>(left_value - right_value);
        } else if (binary_expr_node->op == token::MULT) {
            return Result<int>(left_value * right_value);
        } else if (binary_expr_node->op == token::DIV) {
            if (right_value == 0) {
                return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero." + preprocessor_error,
                "", binary_expr_node->tok));
            }
            return Result<int>(left_value / right_value);
        } else if (binary_expr_node->op == token::MODULO) {
            return Result<int>(left_value % right_value);
        }
        // Add other binary operations here if needed
        return Result<int>(
                CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
                "*, /, -, +, mod", binary_expr_node->tok));
    }
    return Result<int>(CompileError(ErrorType::PreprocessorError,
    "Unknown expression statement. No variables allowed. " + preprocessor_error,
        -1, "", "", ""));
}

PreNodeAST* SimpleExprInterpreter::peek(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, const int ahead) const {
	if (nodes.size() < m_pos+ahead) {
		auto err_msg = "Reached the end of the expression. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, m_line, "", "", m_file).print();
		exit(EXIT_FAILURE);
	}
    if(nodes.size() == m_pos+ahead) return nullptr;
	return nodes.at(m_pos+ahead).get();
}

std::unique_ptr<PreNodeAST> SimpleExprInterpreter::consume(const std::vector<std::unique_ptr<PreNodeAST>>& nodes) {
	if (m_pos < nodes.size()) {
		return nodes.at(m_pos++)->clone();
	}
	auto err_msg = "Reached the end of the m_tokens. Wrong Syntax discovered.";
	CompileError(ErrorType::PreprocessorError, err_msg, m_line, "", "", m_file).print();
	exit(EXIT_FAILURE);
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::parse_binary_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent) {
	auto lhs = _parse_primary_expr(nodes, parent);
	if(!lhs.is_error()) {
		return _parse_binary_expr_rhs(0, std::move(lhs.unwrap()), nodes, parent);
	}
	return Result<std::unique_ptr<PreNodeAST>>(lhs.get_error());
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::_parse_primary_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent) {
	auto current_node = peek(nodes);
	if (auto node_parenth = current_node->cast<PreNodeOther>()) {
		if(node_parenth->tok.type == token::OPEN_PARENTH)
			return _parse_parenth_expr(nodes, parent);
		else if(UNARY_TOKENS.contains(node_parenth->tok.type))
			return parse_unary_expr(nodes, parent);
	} else if (auto node_statement = current_node->cast<PreNodeStatement>()) {
		if (node_statement->statement->cast<PreNodeInt>()) {
            consume(nodes);
			return Result(std::move(node_statement->statement));
		}
	}
	return Result<std::unique_ptr<PreNodeAST>>(CompileError(ErrorType::PreprocessorError,
	"Found unknown expression token. No variables allowed in Preprocessor since statement has to be evaluated during compile time.",
	m_line, "integer, define constant", current_node->get_string(), m_file));
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::parse_unary_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent) {
	auto unary_op = static_cast<PreNodeOther*>(consume(nodes).get())->tok;
	auto node_unary_expr = std::make_unique<PreNodeUnaryExpr>(unary_op, parent);
	auto expr = _parse_primary_expr(nodes, node_unary_expr.get());
	if(expr.is_error()) {
		return Result<std::unique_ptr<PreNodeAST>>(expr.get_error());
	}
	node_unary_expr->operand = std::move(expr.unwrap());
	return Result<std::unique_ptr<PreNodeAST>>(std::move(node_unary_expr));
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::_parse_binary_expr_rhs(int precedence, std::unique_ptr<PreNodeAST> lhs, const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent) {
	while(true) {
		auto current_node = peek(nodes);
		if (!current_node) {
			return Result<std::unique_ptr<PreNodeAST>>(std::move(lhs));
		}
		auto node_other = current_node->cast<PreNodeOther>();
		// auto node_other = dynamic_cast<PreNodeOther*>(peek(nodes));
		int prec = -1;
		if(node_other) prec = _get_binop_precedence(node_other->tok.type);
		if(prec < precedence or !node_other)
			return Result<std::unique_ptr<PreNodeAST>>(std::move(lhs));
		// its not -1 so it is a binop
		auto bin_op = node_other->tok;
		consume(nodes);
		auto rhs = _parse_primary_expr(nodes, parent);
		if (rhs.is_error()) {
			return Result<std::unique_ptr<PreNodeAST>>(rhs.get_error());
		}
		node_other = dynamic_cast<PreNodeOther*>(peek(nodes));
		int next_precedence = -1;
		if(node_other) next_precedence = _get_binop_precedence(node_other->tok.type);
		if (prec < next_precedence or !node_other) {
			rhs = _parse_binary_expr_rhs(prec + 1, std::move(rhs.unwrap()), nodes, parent);
			if (rhs.is_error()) {
				return Result<std::unique_ptr<PreNodeAST>>(rhs.get_error());
			}
		}
		auto node_binary_expr = std::make_unique<PreNodeBinaryExpr>(bin_op.type, std::move(lhs), std::move(rhs.unwrap()), bin_op, parent);
		node_binary_expr->left->parent = node_binary_expr.get();
		node_binary_expr->right->parent = node_binary_expr.get();
		lhs = std::move(node_binary_expr);
	}
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::_parse_parenth_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent) {
	consume(nodes); // eat (
	auto expr = parse_binary_expr(nodes, parent);
	if (expr.is_error()) {
		return Result<std::unique_ptr<PreNodeAST>>(expr.get_error());
	}
	auto current_node = peek(nodes);
	if (!current_node) return expr;
	auto node_other = current_node->cast<PreNodeOther>();
	// auto node_other = dynamic_cast<PreNodeOther*>(peek(nodes));
	if (!node_other or node_other->tok.type != token::CLOSED_PARENTH) {
		auto error = CompileError(ErrorType::PreprocessorError,
			"", "", expr.unwrap()->tok);
		error.set_message("Missing closing parenthesis in expression.");
		error.set_expected(")");
		error.m_got = current_node->get_string();
		return Result<std::unique_ptr<PreNodeAST>>(std::move(error));
		// return Result<std::unique_ptr<PreNodeAST>>(CompileError(ErrorType::PreprocessorError,
		// 	"Missing parenthesis.", m_line, ")", peek(nodes)->get_string(), m_file));
	}
	consume(nodes); // eat )
	return expr;
}

Result<int> SimpleInterpreter::evaluate_int_expression(std::unique_ptr<NodeAST>& root) {
    std::string preprocessor_error = "Statement needs to be evaluated to a single int value before compilation.";
    if (auto int_node = root->cast<NodeInt>()) {
        return Result<int>(int_node->value);
    } else if (auto unary_expr_node = root->cast<NodeUnaryExpr>()) {
        auto operand_value_stmt = evaluate_int_expression(unary_expr_node->operand);
        if (operand_value_stmt.is_error()) return Result<int>(operand_value_stmt.get_error());
        int operandValue = operand_value_stmt.unwrap();
        if (unary_expr_node->op == token::SUB) { // Assuming SUB represents the '-' unary operator
            return Result<int>(-operandValue);
        }
        // Add other unary operations here if needed
        return Result<int>(CompileError(ErrorType::PreprocessorError,
                                        "Unsupported unary operation. " + preprocessor_error,
                                        root->tok.line, "-", get_token_string(unary_expr_node->op), root->tok.file));
    } else if (auto binary_expr_node = root->cast<NodeBinaryExpr>()) {
        auto left_value_stmt = evaluate_int_expression(binary_expr_node->left);
        if (left_value_stmt.is_error()) return Result<int>(left_value_stmt.get_error());
        int left_value = left_value_stmt.unwrap();
        auto right_value_stmt = evaluate_int_expression(binary_expr_node->right);
        if (right_value_stmt.is_error()) return Result<int>(right_value_stmt.get_error());
        int right_value = right_value_stmt.unwrap();
        if (binary_expr_node->op == token::ADD) {
            return Result<int>(left_value + right_value);
        } else if (binary_expr_node->op == token::SUB) {
            return Result<int>(left_value - right_value);
        } else if (binary_expr_node->op == token::MULT) {
            return Result<int>(left_value * right_value);
        } else if (binary_expr_node->op == token::DIV) {
            if (right_value == 0) {
                return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero. " + preprocessor_error,
                                                root->tok.line, "", std::to_string(right_value), root->tok.file));
            }
            return Result<int>(left_value / right_value);
        } else if (binary_expr_node->op == token::MODULO) {
            return Result<int>(left_value % right_value);
        }
        // Add other binary operations here if needed
        return Result<int>(
                CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
                             root->tok.line, "*, /, -, +, mod", token_strings[(int)binary_expr_node->op], root->tok.file));
    }
    return Result<int>(CompileError(ErrorType::PreprocessorError,
                                    "Unknown expression statement. No variables allowed. " + preprocessor_error,
                                    root->tok.line, "", "", root->tok.file));
}

