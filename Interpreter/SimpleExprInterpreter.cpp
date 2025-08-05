
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
    if (auto intNode = dynamic_cast<PreNodeInt *>(root.get())) {
        return Result<int>(intNode->integer);
    } else if (auto unaryExprNode = dynamic_cast<PreNodeUnaryExpr *>(root.get())) {
        auto operandValueStmt = evaluate_int_expression(unaryExprNode->operand);
        if (operandValueStmt.is_error()) return Result<int>(operandValueStmt.get_error());
        int operandValue = operandValueStmt.unwrap();
        if (unaryExprNode->op == token::SUB) { // Assuming SUB represents the '-' unary operator
            return Result<int>(-operandValue);
        }
        // Add other unary operations here if needed
        return Result<int>(CompileError(ErrorType::PreprocessorError,
            "Unsupported unary operation. " + preprocessor_error,
            "-", unaryExprNode->tok));
    } else if (auto binaryExprNode = dynamic_cast<PreNodeBinaryExpr *>(root.get())) {
        auto leftValueStmt = evaluate_int_expression(binaryExprNode->left);
        if (leftValueStmt.is_error()) return Result<int>(leftValueStmt.get_error());
        int leftValue = leftValueStmt.unwrap();
        auto rightValueStmt = evaluate_int_expression(binaryExprNode->right);
        if (rightValueStmt.is_error()) return Result<int>(rightValueStmt.get_error());
        int rightValue = rightValueStmt.unwrap();
        if (binaryExprNode->op == token::ADD) {
            return Result<int>(leftValue + rightValue);
        } else if (binaryExprNode->op == token::SUB) {
            return Result<int>(leftValue - rightValue);
        } else if (binaryExprNode->op == token::MULT) {
            return Result<int>(leftValue * rightValue);
        } else if (binaryExprNode->op == token::DIV) {
            if (rightValue == 0) {
                return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero." + preprocessor_error,
                "", binaryExprNode->tok));
            }
            return Result<int>(leftValue / rightValue);
        } else if (binaryExprNode->op == token::MODULO) {
            return Result<int>(leftValue % rightValue);
        }
        // Add other binary operations here if needed
        return Result<int>(
                CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
                "*, /, -, +, mod", binaryExprNode->tok));
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
	if (auto node_parenth = dynamic_cast<PreNodeOther*>(peek(nodes))) {
		if(node_parenth->tok.type == token::OPEN_PARENTH)
			return _parse_parenth_expr(nodes, parent);
		else if(UNARY_TOKENS.contains(node_parenth->tok.type))
			return parse_unary_expr(nodes, parent);
	} else if (auto node_statement = dynamic_cast<PreNodeStatement*>(peek(nodes))) {
		if (auto node_int = dynamic_cast<PreNodeInt*>(node_statement->statement.get())) {
            consume(nodes);
			return Result(std::move(node_statement->statement));
		}
	}
	return Result<std::unique_ptr<PreNodeAST>>(CompileError(ErrorType::PreprocessorError,
	"Found unknown expression token. No variables allowed in Preprocessor since statement has to be evaluated during compile time.",
	m_line, "integer, define constant", peek(nodes)->get_string(), m_file));
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::parse_unary_expr(const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent) {
	auto unary_op = static_cast<PreNodeOther*>(consume(nodes).get())->tok;
	auto node_unary_expr = std::make_unique<PreNodeUnaryExpr>(unary_op.type, nullptr, unary_op, parent);
	auto expr = _parse_primary_expr(nodes, node_unary_expr.get());
	if(expr.is_error()) {
		return Result<std::unique_ptr<PreNodeAST>>(expr.get_error());
	}
	node_unary_expr->operand = std::move(expr.unwrap());
	return Result<std::unique_ptr<PreNodeAST>>(std::move(node_unary_expr));
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::_parse_binary_expr_rhs(int precedence, std::unique_ptr<PreNodeAST> lhs, const std::vector<std::unique_ptr<PreNodeAST>>& nodes, PreNodeAST *parent) {
	while(true) {
		// auto node_other = peek(nodes)->cast<PreNodeOther>();
		auto node_other = dynamic_cast<PreNodeOther*>(peek(nodes));
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
	auto node_other = dynamic_cast<PreNodeOther*>(peek(nodes));
	if (!node_other or node_other->tok.type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<PreNodeAST>>(CompileError(ErrorType::PreprocessorError,
			"Missing parenthesis.", m_line, ")", peek(nodes)->get_string(), m_file));
	}
	consume(nodes); // eat )
	return expr;
}

Result<int> SimpleInterpreter::evaluate_int_expression(std::unique_ptr<NodeAST>& root) {
    std::string preprocessor_error = "Statement needs to be evaluated to a single int value before compilation.";
    if (auto intNode = dynamic_cast<NodeInt *>(root.get())) {
        return Result<int>(intNode->value);
    } else if (auto unaryExprNode = dynamic_cast<NodeUnaryExpr *>(root.get())) {
        auto operandValueStmt = evaluate_int_expression(unaryExprNode->operand);
        if (operandValueStmt.is_error()) return Result<int>(operandValueStmt.get_error());
        int operandValue = operandValueStmt.unwrap();
        if (unaryExprNode->op == token::SUB) { // Assuming SUB represents the '-' unary operator
            return Result<int>(-operandValue);
        }
        // Add other unary operations here if needed
        return Result<int>(CompileError(ErrorType::PreprocessorError,
                                        "Unsupported unary operation. " + preprocessor_error,
                                        root->tok.line, "-", get_token_string(unaryExprNode->op), root->tok.file));
    } else if (auto binaryExprNode = dynamic_cast<NodeBinaryExpr *>(root.get())) {
        auto leftValueStmt = evaluate_int_expression(binaryExprNode->left);
        if (leftValueStmt.is_error()) return Result<int>(leftValueStmt.get_error());
        int leftValue = leftValueStmt.unwrap();
        auto rightValueStmt = evaluate_int_expression(binaryExprNode->right);
        if (rightValueStmt.is_error()) return Result<int>(rightValueStmt.get_error());
        int rightValue = rightValueStmt.unwrap();
        if (binaryExprNode->op == token::ADD) {
            return Result<int>(leftValue + rightValue);
        } else if (binaryExprNode->op == token::SUB) {
            return Result<int>(leftValue - rightValue);
        } else if (binaryExprNode->op == token::MULT) {
            return Result<int>(leftValue * rightValue);
        } else if (binaryExprNode->op == token::DIV) {
            if (rightValue == 0) {
                return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero. " + preprocessor_error,
                                                root->tok.line, "", std::to_string(rightValue), root->tok.file));
            }
            return Result<int>(leftValue / rightValue);
        } else if (binaryExprNode->op == token::MODULO) {
            return Result<int>(leftValue % rightValue);
        }
        // Add other binary operations here if needed
        return Result<int>(
                CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
                             root->tok.line, "*, /, -, +, mod", token_strings[(int)binaryExprNode->op], root->tok.file));
    }
    return Result<int>(CompileError(ErrorType::PreprocessorError,
                                    "Unknown expression statement. No variables allowed. " + preprocessor_error,
                                    root->tok.line, "", "", root->tok.file));
}

