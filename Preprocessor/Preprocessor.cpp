//
// Created by Mathias Vatter on 23.09.23.
//

#include <filesystem>
#include <iostream>
#include "Preprocessor.h"
#include "PreprocessorImport.h"
#include "PreprocessorMacros.h"

Preprocessor::Preprocessor(std::vector<Token> tokens, std::string current_file)
    : Parser(std::move(tokens)), m_current_file(std::move(current_file)) {
}

void Preprocessor::process() {
	Result<SuccessTag> result = Result<SuccessTag>(SuccessTag{});

    PreprocessorImport imports(m_tokens, m_current_file);
	result = imports.process_imports();
	if(result.is_error()) {
		result.get_error().print();
		auto err_msg = "Preprocessor failed while processing import statements.";
		CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek(m_tokens).file).print();
		exit(EXIT_FAILURE);
	}
    m_tokens = std::move(imports.get_tokens());

    PreprocessorMacros macros(m_tokens, m_current_file);
    result = macros.process_macros();
    if(result.is_error()) {
        result.get_error().print();
        auto err_msg = "Preprocessor failed while processing macros.";
        CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek(m_tokens).file).print();
        exit(EXIT_FAILURE);
    }
    m_tokens = std::move(macros.get_tokens());
}

std::vector<Token> Preprocessor::get_tokens() {
    return std::move(m_tokens);
}

Token Preprocessor::peek(const std::vector<Token>& tok, int ahead) {
	if (tok.size() < m_pos+ahead) {
		auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).print();
		exit(EXIT_FAILURE);
	}
	m_curr_token = tok.at(m_pos).type;
    m_curr_token_value = tok.at(m_pos).val;
	return tok.at(m_pos+ahead);

}

Token Preprocessor::consume(const std::vector<Token>& tok) {
	if (m_pos < tok.size()) {
		m_curr_token = tok.at(m_pos + 1).type;
        m_curr_token_value = tok.at(m_pos+1).val;
		return tok.at(m_pos++);
	}
	auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
	CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).print();
	exit(EXIT_FAILURE);
}

const Token& Preprocessor::get_tok(const std::vector<Token>& tok) const {
	return tok.at(m_pos);
}

void Preprocessor::remove_last() {
    if (m_pos > 0 && m_pos <= m_tokens.size()) {
        m_tokens.erase(m_tokens.begin() + m_pos - 1);
        m_pos--;  // Adjust m_pos since we've removed an element before it
    } else {
        auto err_msg = "Attempted to remove a token out of bounds.";
        CompileError(ErrorType::PreprocessorError, err_msg, m_tokens.at(m_pos).line, "unknown", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
}

void Preprocessor::remove_tokens(std::vector<Token>& tok, size_t start, size_t end) {
    if (start < end && end <= tok.size()) {
        tok.erase(tok.begin() + start, tok.begin() + end);
        // Adjust m_pos if necessary
        if (m_pos > start)
            m_pos -= (end - start);
    } else {
        auto err_msg = "Attempted to remove a token range out of bounds.";
        CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "unknown", tok.at(m_pos).val, tok.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
}




Result<std::unique_ptr<NodeAST>> Preprocessor::parse_int(const std::vector<Token>& tok) {
	auto token = consume(tok);
	auto value = token.val;
	try {
		long long val = std::stoll(value, nullptr, 10);
		auto node = std::make_unique<NodeInt>(static_cast<int32_t>(val & 0xFFFFFFFF), token);
		node->type = ASTType::Integer;
		return Result<std::unique_ptr<NodeAST>>(std::move(node));
	} catch (const std::exception& e) {
		auto expected = std::string(1, "valid int base "[10]);
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::PreprocessorError,
		 "Invalid integer format.", token.line, expected, value, token.file));
	}
}

Result<std::unique_ptr<NodeAST>> Preprocessor::parse_binary_expr(const std::vector<Token>& tok) {
	auto lhs = _parse_primary_expr(tok);
	if(!lhs.is_error()) {
		return _parse_binary_expr_rhs(0, std::move(lhs.unwrap()), tok);
	}
	return Result<std::unique_ptr<NodeAST>>(lhs.get_error());
}

Result<std::unique_ptr<NodeAST>> Preprocessor::_parse_primary_expr(const std::vector<Token>& tok) {
	if (peek(tok).type == token::OPEN_PARENTH) {
		return _parse_parenth_expr(tok);
	} else if (peek(tok).type == token::INT) {
		return parse_int(tok);
		// unary operators bool_not, bit_not, sub
	} else if (is_unary_operator(peek(tok).type)){
		return parse_unary_expr(tok);
	} else {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::PreprocessorError,
		 "Found unknown expression token. No variables allowed in Preprocessor since statement has to be evaluated during compile time.",
		 peek(tok).line, "integer, define constant", peek(tok).val, peek(tok).file));
	}
}

Result<std::unique_ptr<NodeAST>> Preprocessor::parse_unary_expr(const std::vector<Token>& tok) {
	Token unary_op = consume(tok);
	auto expr = parse_binary_expr(tok);
	if(expr.is_error()) {
		return Result<std::unique_ptr<NodeAST>>(expr.get_error());
	}
	auto return_value = std::make_unique<NodeUnaryExpr>(unary_op, std::move(expr.unwrap()), get_tok(tok));
	return Result<std::unique_ptr<NodeAST>>(std::move(return_value));
}

Result<std::unique_ptr<NodeAST>> Preprocessor::_parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs, const std::vector<Token>& tok) {
	while(true) {
		int prec = _get_binop_precedence(peek(tok).type);
		if(prec < precedence)
			return Result<std::unique_ptr<NodeAST>>(std::move(lhs));
		// its not -1 so it is a binop
		auto bin_op = peek(tok);
		consume(tok);
		auto rhs = _parse_primary_expr(tok);
		if (rhs.is_error()) {
			return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
		}
		int next_precedence = _get_binop_precedence(peek(tok).type);
		if (prec < next_precedence) {
			rhs = _parse_binary_expr_rhs(prec + 1, std::move(rhs.unwrap()), tok);
			if (rhs.is_error()) {
				return Result<std::unique_ptr<NodeAST>>(rhs.get_error());
			}
		}
		lhs = std::make_unique<NodeBinaryExpr>(bin_op.val, std::move(lhs), std::move(rhs.unwrap()), get_tok(tok));
	}
}

Result<std::unique_ptr<NodeAST>> Preprocessor::_parse_parenth_expr(const std::vector<Token>& tok) {
	consume(tok); // eat (
	auto expr = parse_binary_expr(tok);
	if (peek(tok).type != token::CLOSED_PARENTH) {
		return Result<std::unique_ptr<NodeAST>>(CompileError(ErrorType::PreprocessorError,
		 "Missing parenthesis.", peek(tok).line, ")", peek(tok).val, peek(tok).file));
	}
	consume(tok); // eat )
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
		if (unaryExprNode->op.type == token::SUB) { // Assuming SUB represents the '-' unary operator
			return Result<int>(-operandValue);
		}
		// Add other unary operations here if needed
		return Result<int>(CompileError(ErrorType::PreprocessorError,
										"Unsupported unary operation. " + preprocessor_error,
										root->tok.line, "-", unaryExprNode->op.val, root->tok.file));
	} else if (auto binaryExprNode = dynamic_cast<NodeBinaryExpr *>(root.get())) {
		auto leftValueStmt = evaluate_int_expression(binaryExprNode->left);
		if (leftValueStmt.is_error()) return Result<int>(leftValueStmt.get_error());
		int leftValue = leftValueStmt.unwrap();
		auto rightValueStmt = evaluate_int_expression(binaryExprNode->right);
		if (rightValueStmt.is_error()) return Result<int>(rightValueStmt.get_error());
		int rightValue = rightValueStmt.unwrap();
		if (binaryExprNode->op == "+") {
			return Result<int>(leftValue + rightValue);
		} else if (binaryExprNode->op == "-") {
			return Result<int>(leftValue - rightValue);
		} else if (binaryExprNode->op == "*") {
			return Result<int>(leftValue * rightValue);
		} else if (binaryExprNode->op == "/") {
			if (rightValue == 0) {
				return Result<int>(CompileError(ErrorType::PreprocessorError, "Divison by zero. " + preprocessor_error,
												root->tok.line, "", std::to_string(rightValue), root->tok.file));
			}
			return Result<int>(leftValue / rightValue);
		} else if (binaryExprNode->op == "mod") {
			return Result<int>(leftValue % rightValue);
		}
		// Add other binary operations here if needed
		return Result<int>(
			CompileError(ErrorType::PreprocessorError, "Unsupported binary operation. " + preprocessor_error,
						 root->tok.line, "*, /, -, +, mod", binaryExprNode->op, root->tok.file));
	}
	return Result<int>(CompileError(ErrorType::PreprocessorError,
									"Unknown expression statement. No variables allowed. " + preprocessor_error,
									root->tok.line, "", "", root->tok.file));
}


