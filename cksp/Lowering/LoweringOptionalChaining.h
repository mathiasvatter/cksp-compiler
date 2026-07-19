//
// Created by Mathias Vatter on 19.07.26.
//

#pragma once

#include "ASTLowering.h"

/**
 * Lowers optional chaining:
 * pre:
 *	obj?.method(x)
 *  obj?.member := 5
 * post:
 * if obj # nil
 *  obj.method(x) // x will not be evaluated if obj = nil -> short-cuircuiting
 * end if
 * if obj # nil
 *  obj.member := 5
 * end if
 */
class LoweringOptionalChaining final : public ASTLowering {
	NodeAST* start_pointer = nullptr;
	Type* prev_type = nullptr;
public:
	explicit LoweringOptionalChaining(NodeProgram *program) : ASTLowering(program) {}



	NodeAST * visit(NodeAccessChain& node) override {
		if (!node.has_opt_chaining()) {
			return &node;
		}
		// only this case gets handled here:
		// obj?.method(x)
		if (!node.chain.back()->cast<NodeFunctionCall>()) return &node;

		Token curr_opt_chaining;
		auto idx = 0;
		for (size_t i = 0; i< node.chain.size(); i++) {
			auto& tok = node.opt_chaining_indexes[i];
			if (tok.has_value()) {
				curr_opt_chaining = tok.value();
				idx = i;
				break;
			}
		}
		auto first = node.split(idx+1);
		auto condition = std::make_unique<NodeBinaryExpr>(
			token::NOT_EQUAL,
			std::move(first),
			std::make_unique<NodeNil>(curr_opt_chaining),
			curr_opt_chaining
		);
		auto block = std::make_unique<NodeBlock>(node.tok, true);
		auto node_access = std::make_unique<NodeAccessChain>(std::move(node.chain), node.tok);
		node_access->declaration = node.declaration;
		node_access->ty = node.ty;
		block->add_as_stmt(std::move(node_access));
		auto node_if = std::make_unique<NodeIf>(
			std::move(condition),
			std::move(block),
			std::make_unique<NodeBlock>(node.tok),
			node.tok
		);
		return node.replace_with(std::move(node_if))->accept(*this);
	}

	NodeAST * visit(NodeSingleAssignment& node) override {
		// if l_value is access chain with optional chaining:
		// obj?.member := 5
		auto l_access = node.l_value->cast<NodeAccessChain>();
		if (l_access->has_opt_chaining()) {

		}


		node.r_value->accept(*this);
		node.l_value->accept(*this);


		return &node;
	}


};