//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Second Lowering Phase of search
 * has to happen after function inlining
 * transformation of separate Node NodeSortSearch into NodeFunctionCall
 */
class PostLoweringSortSearch final : public ASTLowering {

public:
	explicit PostLoweringSortSearch(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeSortSearch &node) override {
		// search(array, value, from, to)
		auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				node.name,
				std::make_unique<NodeParamList>(node.tok, std::move(node.array), std::move(node.value)),
				node.tok
			),
			node.tok
		);
		if(node.from) func_call->function->add_arg(std::move(node.from));
		if(node.to) func_call->function->add_arg(std::move(node.to));

		func_call->ty = node.ty;
		func_call->kind = NodeFunctionCall::Kind::Builtin;
		func_call->bind_definition(m_program);

		return node.replace_with(std::move(func_call));
	}

};