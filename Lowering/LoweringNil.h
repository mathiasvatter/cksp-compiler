//
// Created by Mathias Vatter on 19.07.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringNil : public ASTLowering {
private:

public:
	explicit LoweringNil(NodeProgram *program) : ASTLowering(program) {}

	inline NodeAST * visit(NodeNil &node) override {
		std::unique_ptr<NodeAST> node_repl = nullptr;
		if(node.is_string_env()) {
			node_repl = std::make_unique<NodeString>("\"nil\"", node.tok);
		} else {
			// replace nil with -1
			node_repl = std::make_unique<NodeUnaryExpr>(
				  token::SUB,
				  std::make_unique<NodeInt>(1, node.tok),
				  node.tok
			  );
		}
		return node.replace_with(std::move(node_repl));
	}

};