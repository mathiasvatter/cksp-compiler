
#include "SimpleExprInterpreter.h"
#include "Parser.h"

SimpleExprInterpreter::SimpleExprInterpreter(std::unique_ptr<PreNodeChunk> chunk) {
	m_nodes = std::move(chunk->chunk);
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::parse() {
	auto root_result = parse_binary_expr(nullptr);
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
        if (unaryExprNode->op.type == token::SUB) { // Assuming SUB represents the '-' unary operator
            return Result<int>(-operandValue);
        }
        // Add other unary operations here if needed
        return Result<int>(CompileError(ErrorType::PreprocessorError,
            "Unsupported unary operation. " + preprocessor_error,
            unaryExprNode->op.line, "-", unaryExprNode->op.val, unaryExprNode->op.file));
    } else if (auto binaryExprNode = dynamic_cast<PreNodeBinaryExpr *>(root.get())) {
        auto leftValueStmt = evaluate_int_expression(binaryExprNode->left);
        if (leftValueStmt.is_error()) return Result<int>(leftValueStmt.get_error());
        int leftValue = leftValueStmt.unwrap();
        auto rightValueStmt = evaluate_int_expression(binaryExprNode->right);
        if (rightValueStmt.is_error()) return Result<int>(rightValueStmt.get_error());
        int rightValue = rightValueStmt.unwrap();
        if (binaryExprNode->op.type == ADD) {
            return Result<int>(leftValue + rightValue);
        } else if (binaryExprNode->op.type == SUB) {
            return Result<int>(leftValue - rightValue);
        } else if (binaryExprNode->op.type == MULT) {
            return Result<int>(leftValue * rightValue);
        } else if (binaryExprNode->op.type == DIV) {
            if (rightValue == 0) {
                return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero." + preprocessor_error,
                binaryExprNode->op.line, "", std::to_string(rightValue), binaryExprNode->op.file));
            }
            return Result<int>(leftValue / rightValue);
        } else if (binaryExprNode->op.type == MODULO) {
            return Result<int>(leftValue % rightValue);
        }
        // Add other binary operations here if needed
        return Result<int>(
                CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
                             binaryExprNode->op.line, "*, /, -, +, mod", binaryExprNode->op.val, binaryExprNode->op.file));
    }
    return Result<int>(CompileError(ErrorType::PreprocessorError,
    "Unknown expression statement. No variables allowed. " + preprocessor_error,
        -1, "", "", ""));
}

PreNodeAST* SimpleExprInterpreter::peek(int ahead) {
	if (m_nodes.size() < m_pos+ahead) {
		auto err_msg = "Reached the end of the expression. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, m_line, "", "", m_file).print();
		exit(EXIT_FAILURE);
	}
	return m_nodes.at(m_pos+ahead).get();

}

std::unique_ptr<PreNodeAST> SimpleExprInterpreter::consume() {
	if (m_pos < m_nodes.size()) {
		return std::move(m_nodes.at(m_pos++));
	}
	auto err_msg = "Reached the end of the m_tokens. Wrong Syntax discovered.";
	CompileError(ErrorType::PreprocessorError, err_msg, m_line, "", "", m_file).print();
	exit(EXIT_FAILURE);
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::parse_binary_expr(PreNodeAST *parent) {
	auto lhs = _parse_primary_expr(parent);
	if(!lhs.is_error()) {
		return _parse_binary_expr_rhs(0, std::move(lhs.unwrap()), parent);
	}
	return Result<std::unique_ptr<PreNodeAST>>(lhs.get_error());
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::_parse_primary_expr(PreNodeAST *parent) {
	if (auto node_parenth = dynamic_cast<PreNodeOther*>(peek())) {
		if(node_parenth->other.type == OPEN_PARENTH)
			return _parse_parenth_expr(parent);
		else if(is_unary_operator(node_parenth->other.type))
			return parse_unary_expr(parent);
	} else if (auto node_statement = dynamic_cast<PreNodeStatement*>(peek())) {
		if (auto node_int = dynamic_cast<PreNodeInt*>(node_statement->statement.get())) {
			return Result(consume());
		}
	}
	return Result<std::unique_ptr<PreNodeAST>>(CompileError(ErrorType::PreprocessorError,
	"Found unknown expression token. No variables allowed in Preprocessor since statement has to be evaluated during compile time.",
	m_line, "integer, define constant", peek()->get_string(), m_file));
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::parse_unary_expr(PreNodeAST *parent) {
	auto unary_op = static_cast<PreNodeOther*>(consume().get())->other;
	auto node_unary_expr = std::make_unique<PreNodeUnaryExpr>(unary_op, nullptr, parent);
	auto expr = _parse_primary_expr(node_unary_expr.get());
	if(expr.is_error()) {
		return Result<std::unique_ptr<PreNodeAST>>(expr.get_error());
	}
	node_unary_expr->operand = std::move(expr.unwrap());
	return Result<std::unique_ptr<PreNodeAST>>(std::move(node_unary_expr));
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::_parse_binary_expr_rhs(int precedence, std::unique_ptr<PreNodeAST> lhs, PreNodeAST *parent) {
	while(true) {
		auto node_other = dynamic_cast<PreNodeOther*>(peek());
		int prec = -1;
		if(node_other) prec = _get_binop_precedence(node_other->other.type);
		if(prec < precedence or !node_other)
			return Result<std::unique_ptr<PreNodeAST>>(std::move(lhs));
		// its not -1 so it is a binop
		auto bin_op = node_other->other;
		consume();
		auto rhs = _parse_primary_expr(parent);
		if (rhs.is_error()) {
			return Result<std::unique_ptr<PreNodeAST>>(rhs.get_error());
		}
		node_other = dynamic_cast<PreNodeOther*>(peek());
		int next_precedence = -1;
		if(node_other) next_precedence = _get_binop_precedence(node_other->other.type);
		if (prec < next_precedence or !node_other) {
			rhs = _parse_binary_expr_rhs(prec + 1, std::move(rhs.unwrap()), parent);
			if (rhs.is_error()) {
				return Result<std::unique_ptr<PreNodeAST>>(rhs.get_error());
			}
		}
		auto node_binary_expr = std::make_unique<PreNodeBinaryExpr>(bin_op, std::move(lhs), std::move(rhs.unwrap()),parent);
		node_binary_expr->left->parent = node_binary_expr.get();
		node_binary_expr->right->parent = node_binary_expr.get();
		lhs = std::move(node_binary_expr);
	}
}

Result<std::unique_ptr<PreNodeAST>> SimpleExprInterpreter::_parse_parenth_expr(PreNodeAST *parent) {
	consume(); // eat (
	auto expr = parse_binary_expr(parent);
	auto node_other = dynamic_cast<PreNodeOther*>(peek());
	if (!node_other or node_other->other.type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<PreNodeAST>>(CompileError(ErrorType::PreprocessorError,
			"Missing parenthesis.", m_line, ")", peek()->get_string(), m_file));
	}
	consume(); // eat )
	return expr;
}
