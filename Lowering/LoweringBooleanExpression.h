//
// Created by Mathias Vatter on 07.09.25.
//

#pragma once

#include "ASTLowering.h"

/**
 * This class lowers expressions with comparisons and booleans into integer expressions by using the
 * provided boolean functions und substituing the operators.
 * For example:
 * a > b and c < d
 * -->
 * CKSP::__gt__(a, b) .and. CKSP::__lt__(c, d)
 */
class LoweringBooleanExpression final : public ASTLowering {
	static inline std::unordered_map<token, token> boolean_to_bitwise = {
		{token::BOOL_AND, token::BIT_AND},
		{token::BOOL_OR, token::BIT_OR},
		{token::BOOL_XOR, token::BIT_XOR},
		{token::BOOL_NOT, token::BIT_NOT}
	};
public:
	explicit LoweringBooleanExpression(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * lower_expression(NodeAST& node) {
		if (!node.parent) return &node;
		if (node.parent->cast<NodeWhile>() or node.parent->cast<NodeIf>() or node.parent->cast<NodeTernary>()) {
			// do not lower conditions in if and while statements
			return &node;
		}
		if (node.parent->cast<NodeBinaryExpr>() or node.parent->cast<NodeUnaryExpr>()) {
			// do not lower if we are not at the start of an expression
			return &node;
		}
		node.accept(*this);
		return &node;
	}

	NodeAST* visit(NodeUnaryExpr& node) override {
		node.operand->accept(*this);

		// check if it is a boolean operator
		if (not BOOL_TOKENS.contains(node.op)) {
			return &node;
		}

		// if boolean operator, replace operand with 1-operand
		// e.g. not a --> 1 - a
		// only NOT is supported as unary boolean operator
		if (node.op == token::BOOL_NOT) {
			auto expression = std::make_unique<NodeBinaryExpr>(
				token::SUB,
				std::make_unique<NodeInt>(1, node.tok),
				std::move(node.operand),
				node.tok
			);
			expression->ty = TypeRegistry::Integer;
			expression->left->set_element_type(TypeRegistry::Integer);
			return node.replace_with(std::move(expression));
		} else {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "<LoweringBooleanExpression>: Unsupported unary boolean operator.";
			error.exit();
		}

		return &node;
	}

	NodeAST* visit(NodeBinaryExpr& node) override {
		node.left->accept(*this);
		node.right->accept(*this);

		// check if it is a boolean operator
		if (not(BOOL_TOKENS.contains(node.op) || COMPARISON_TOKENS.contains(node.op))) {
			return &node;
		}

		// if boolean operator, replace with bitwise operator
		if (BOOL_TOKENS.contains(node.op)) {
			const auto it = boolean_to_bitwise.find(node.op);
			if (it == boolean_to_bitwise.end()) {
				auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
				error.m_message = "<LoweringBooleanExpression>: Boolean operator not found in mapping.";
				error.exit();
			}
			node.op = it->second;
			node.ty = TypeRegistry::Integer;
			node.left->set_element_type(TypeRegistry::Integer);
			node.right->set_element_type(TypeRegistry::Integer);
			return &node;
		}

		// if comparison operator, replace with function call
		if (COMPARISON_TOKENS.contains(node.op)) {
			const auto it = BOOLEAN_FUNCTIONS.find(node.op);
			if (it == BOOLEAN_FUNCTIONS.end()) {
				auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
				error.m_message = "<LoweringBooleanExpression>: Comparison operator not found in mapping.";
				error.exit();
			}
			auto name = it->second.first;
			auto num_params = it->second.second;
			// auto func = m_def_provider->get_boolean_function(name, num_params);
			// if (!func) {
			// 	auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			// 	error.m_message = "<LoweringBooleanExpression>: Boolean function " + it->second.first + " not found.";
			// 	error.exit();
			// }

			node.left->set_element_type(TypeRegistry::Integer);
			node.right->set_element_type(TypeRegistry::Integer);
			auto call = DefinitionProvider::create_builtin_call(
				name,
				std::move(node.left),
				std::move(node.right)
			);
			call->bind_definition(m_program);
			// call->definition = func;
			// call->strategy = NodeFunctionCall::Strategy::ExpressionFunc;
			return node.replace_with(std::move(call));
		}


		return &node;
	}
};