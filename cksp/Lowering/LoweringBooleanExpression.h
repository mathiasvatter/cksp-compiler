//
// Created by Mathias Vatter on 07.09.25.
//

#pragma once

#include "ASTLowering.h"

/**
 * This class lowers expressions with comparisons and booleans into integer expressions by using the
 * provided boolean functions und substituting the operators.
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

	/// <message(a < b)>: the boolean prints as "true" or "false", chosen by a ternary on it.
	NodeAST* replace_with_text_ternary(NodeAST& node, std::unique_ptr<NodeAST> condition) {
		auto ternary = std::make_unique<NodeTernary>(
			std::move(condition),
			std::make_unique<NodeString>("\"true\"", node.tok),
			std::make_unique<NodeString>("\"false\"", node.tok),
			node.tok
		);
		ternary->if_branch->do_type_inference(m_program);
		ternary->else_branch->do_type_inference(m_program);
		return node.replace_with(std::move(ternary))->do_lowering(m_program);
	}

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
		// Checked before the operand is lowered: the operand becomes the condition of the
		// ternary, where comparisons stay comparisons.
		if (BOOL_TOKENS.contains(node.op.type) and node.is_string_env()) {
			auto condition = std::make_unique<NodeUnaryExpr>(node.op, std::move(node.operand), node.tok);
			return replace_with_text_ternary(node, std::move(condition));
		}

		node.operand->accept(*this);

		// check if it is a boolean operator
		if (not BOOL_TOKENS.contains(node.op.type)) {
			return &node;
		}

		// if boolean operator, replace operand with 1-operand
		// e.g. not a --> 1 - a
		// only NOT is supported as unary boolean operator
		if (node.op.type == token::BOOL_NOT) {
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
			auto error = Diagnostic(ErrorType::InternalError, "", "", node.tok);
			error.message = "<LoweringBooleanExpression>: Unsupported unary boolean operator.";
			error.exit();
		}

		return &node;
	}

	NodeAST* visit(NodeBinaryExpr& node) override {
		const bool is_boolean = BOOL_TOKENS.contains(node.op.type) || COMPARISON_TOKENS.contains(node.op.type);

		// Checked before the operands are lowered: they become the condition of the ternary,
		// where <a < b> stays a comparison instead of turning into its integer value.
		if (is_boolean and node.is_string_env()) {
			auto condition = std::make_unique<NodeBinaryExpr>(
				node.op, std::move(node.left), std::move(node.right), node.tok
			);
			return replace_with_text_ternary(node, std::move(condition));
		}

		node.left->accept(*this);
		node.right->accept(*this);

		// check if it is a boolean operator
		if (not is_boolean) {
			return &node;
		}

		// if boolean operator, replace with bitwise operator
		if (BOOL_TOKENS.contains(node.op.type)) {
			const auto it = boolean_to_bitwise.find(node.op.type);
			if (it == boolean_to_bitwise.end()) {
				auto error = Diagnostic(ErrorType::InternalError, "", "", node.tok);
				error.message = "<LoweringBooleanExpression>: Boolean operator not found in mapping.";
				error.exit();
			}
			node.op.type = it->second;
			node.ty = TypeRegistry::Integer;
			node.left->set_element_type(TypeRegistry::Integer);
			node.right->set_element_type(TypeRegistry::Integer);
			return &node;
		}

		// if comparison operator, replace with function call
		if (COMPARISON_TOKENS.contains(node.op.type)) {
			const auto it = BOOLEAN_FUNCTIONS.find(node.op.type);
			if (it == BOOLEAN_FUNCTIONS.end()) {
				auto error = Diagnostic(ErrorType::InternalError, "", "", node.tok);
				error.message = "<LoweringBooleanExpression>: Comparison operator not found in mapping.";
				error.exit();
			}
			auto name = it->second.first;
			auto num_params = it->second.second;
			// auto func = m_def_provider->get_boolean_function(name, num_params);
			// if (!func) {
			// 	auto error = Diagnostic(ErrorType::InternalError, "", "", node.tok);
			// 	error.message = "<LoweringBooleanExpression>: Boolean function " + it->second.first + " not found.";
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