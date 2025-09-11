//
// Created by Mathias Vatter on 19.07.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringNil final : public ASTLowering {

public:
	explicit LoweringNil(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeNil &node) override {
		std::unique_ptr<NodeAST> node_repl = nullptr;
		if(node.is_string_env()) {
			node_repl = std::make_unique<NodeString>("\"nil\"", node.tok);
			node_repl->ty = TypeRegistry::String;
		} else {
			// replace nil with -1
			node_repl = std::make_unique<NodeUnaryExpr>(
				  token::SUB,
				  std::make_unique<NodeInt>(1, node.tok),
				  node.tok
			  );
			node_repl->ty = TypeRegistry::Integer;
		}
		return node.replace_with(std::move(node_repl));
	}

};