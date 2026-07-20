//
// Created by Mathias Vatter on 13.09.25.
//

#pragma once

#include "ASTLowering.h"
#include "../SyntaxChecks/KSPConditions.h"

/**
 * This class is for lowering comparisons in if and while conditions by bringing single booleans that are not
 * part of a comparison into comparisons against zero.
 * For example:
 * if myVar
 * is lowered to
 * if myVar # 0
 *
 * Should be applied right after lowering boolean types into integer types
 */
class LoweringComparisons final : public ASTLowering {

public:
	explicit LoweringComparisons(NodeProgram *program) : ASTLowering(program) {}


	NodeAST* lower_comparison(std::unique_ptr<NodeAST>& condition) {
		if (condition->cast<NodeBinaryExpr>() or condition->cast<NodeUnaryExpr>()) {
			condition->accept(*this);
		} else {
		// condition is not a binary or unary expression -> must be a single literal or reference or function call
			KSPConditions::sanitize(condition, condition->parent);
		}
		return condition.get();
	}


private:

	NodeAST* visit(NodeBinaryExpr& node) override {
		node.left->accept(*this);
		node.right->accept(*this);

		if (not BOOL_TOKENS.contains(node.op)) {
			return &node;
		}

		if (node.left->ty == TypeRegistry::Integer) {
			auto comparison = std::make_unique<NodeBinaryExpr>(
				token::NOT_EQUAL,
				std::move(node.left),
				std::make_unique<NodeInt>(0, node.tok),
				node.tok
			);
			node.set_left(std::move(comparison));
		}
		if (node.right->ty == TypeRegistry::Integer) {
			auto comparison = std::make_unique<NodeBinaryExpr>(
				token::NOT_EQUAL,
				std::move(node.right),
				std::make_unique<NodeInt>(0, node.tok),
				node.tok
			);
			node.set_right(std::move(comparison));
		}

		return &node;
	}

	NodeAST *visit(NodeUnaryExpr &node) override {
		node.operand->accept(*this);

		if (not BOOL_TOKENS.contains(node.op)) {
			return &node;
		}

		if (node.operand->ty == TypeRegistry::Integer) {
			auto comparison = std::make_unique<NodeBinaryExpr>(
				token::NOT_EQUAL,
				std::move(node.operand),
				std::make_unique<NodeInt>(0, node.tok),
				node.tok
			);
			node.set_operand(std::move(comparison));
		}

		return &node;
	}

	/// IMPORTANT: Kontakt does not accept comparisons on builtin functions that have a boolean return type
	/// such as array_equal(arr1, arr2) or pgs_key_exists
	/// so we need to throw an error if the user does use a comparison here! -> pgs_key_exists(PGS) = 1 <- KSP error!
	NodeAST *visit(NodeFunctionCall &node) override {
		auto bin_exp = node.parent->cast<NodeBinaryExpr>();
		if (!bin_exp) return &node;

		if (not COMPARISON_TOKENS.contains(bin_exp->op)) {
			return &node;
		}

		if (node.is_builtin_kind() and node.ty->get_element_type() == TypeRegistry::Boolean) {
			auto other = bin_exp->left.get() == &node ? bin_exp->right.get() : bin_exp->left.get();
			auto error = Diagnostic(ErrorType::SyntaxError, "", "", other->tok);
			error.message = "Comparison operators cannot be used on builtin functions that return boolean values such as <"+node.function->name+">.";
			error.exit();
		}
		return ASTVisitor::visit(node);
	}

};