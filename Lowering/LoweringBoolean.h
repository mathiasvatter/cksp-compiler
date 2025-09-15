//
// Created by Mathias Vatter on 13.09.25.
//

#pragma once

#include "ASTLowering.h"

class LoweringBoolean final : public ASTLowering {

public:
	explicit LoweringBoolean(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeBoolean &node) override {
		std::unique_ptr<NodeAST> node_repl = nullptr;
		static auto bool_tokens_map = invert_map(BOOLEAN_SYNTAX);
		if(node.is_string_env()) {
			auto it = bool_tokens_map.find(node.tok.type);
			if (it == bool_tokens_map.end()) {
				auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
				error.m_message = "<LoweringBoolean>: Boolean value not found in mapping.";
				error.exit();
			}
			node_repl = std::make_unique<NodeString>("\""+it->second + "\"", node.tok);
			node_repl->ty = TypeRegistry::String;
		} else {
			// replace boolean with int 1 or 0
			node_repl = std::make_unique<NodeInt>(
				node.value ? 1 : 0,
				node.tok
			);
			node_repl->ty = TypeRegistry::Integer;
		}
		return node.replace_with(std::move(node_repl));
	}

};