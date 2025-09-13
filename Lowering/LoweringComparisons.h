//
// Created by Mathias Vatter on 13.09.25.
//

#pragma once

#include "ASTLowering.h"

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

	NodeAST* lower_comparison(NodeAST& condition) {
		return condition.accept(*this);
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

};