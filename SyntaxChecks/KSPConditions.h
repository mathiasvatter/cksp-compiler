//
// Created by Mathias Vatter on 13.09.25.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../AST/ASTVisitor/GlobalScope/NormalizeArrayAssign.h"

/**
 * Sanitizes conditions in if and while statements to be valid KSP expressions.
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

private:

	static void sanitize(std::unique_ptr<NodeAST>& condition, NodeAST* parent) {
		auto tok = condition->tok;
		if (not(condition->is_literal() || condition->is_reference())) {
			return;
		}

		auto condition_type = condition->ty;
		if (condition_type == TypeRegistry::Unknown) {
			auto error = CompileError(ErrorType::InternalError, "", "", tok);
			error.m_message = "Condition has unknown type. This should not happen.";
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