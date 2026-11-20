//
// Created by Mathias Vatter on 13.09.25.
//

#pragma once

#include "../ASTVisitor/ASTVisitor.h"

/**
 * Sanitizes conditions in if and while statements that are NOT unary or binary expressions to be valid KSP expressions.
 * (so conditions that are only literals or var references or non-builtin function calls)
 * Normally, conditions that are not comparisons or boolean expressions are not valid in KSP.
 * This class ensures that such conditions (only consisting of var references or literals) are lowered
 * into comparisons against zero.
 * For example:
 * if myVar
 * is lowered to
 * if myVar # 0
 */
class KSPConditions final : public ASTVisitor {

public:
	static NodeAST* sanitize_condition(NodeWhile& node) {
		sanitize(node.condition, &node);
		return &node;
	}

	static NodeAST* sanitize_condition(NodeIf& node) {
		sanitize(node.condition, &node);
		return &node;
	}

	/// Decided by the operator, not the type: after type lowering a comparison is typed as an integer.
	/// Arithmetic like <-a> or <a .and. b> is a binary or unary expression too, and still needs <# 0>.
	static bool is_boolean_expression(NodeAST& condition) {
		if (const auto bin = condition.cast<NodeBinaryExpr>()) {
			return COMPARISON_TOKENS.contains(bin->op.type) or BOOL_TOKENS.contains(bin->op.type);
		}
		if (const auto unary = condition.cast<NodeUnaryExpr>()) {
			return BOOL_TOKENS.contains(unary->op.type);
		}
		return false;
	}

	static void sanitize(std::unique_ptr<NodeAST>& condition, NodeAST* parent) {
		auto tok = condition->tok;
		if (is_boolean_expression(*condition)) {
			// condition is already a comparison or boolean expression -> do nothing
			return;
		}
		if (auto call = condition->cast<NodeFunctionCall>()) {
			// condition is a builtin function call -> do nothing IF return type is bool
			// if return type is not bool, Kontakt wants a comparison
			if (call->is_builtin_kind() and call->ty->get_element_type() == TypeRegistry::Boolean) {
				return;
			}
		}

		auto condition_type = condition->ty;
		if (condition_type == TypeRegistry::Unknown) {
			auto error = Diagnostic(ErrorType::InternalError, "", "", tok);
			error.message = "Condition has unknown type. This should not happen.";
			error.exit();
		}

		auto comparison = std::make_unique<NodeBinaryExpr>(
			token::NOT_EQUAL,
			std::move(condition),
			std::make_unique<NodeInt>(0, tok),
			tok
		);
		if (condition_type == TypeRegistry::Integer) {
			comparison->right->replace_with(std::make_unique<NodeInt>(0, tok));
		} else if (condition_type == TypeRegistry::Real) {
			comparison->right->replace_with(std::make_unique<NodeReal>(0, tok));
		} else if (condition_type == TypeRegistry::String) {
			comparison->left->replace_with(std::make_unique<NodeInt>(1, tok));
		}

		comparison->parent = parent;
		condition = std::move(comparison);

	}
};