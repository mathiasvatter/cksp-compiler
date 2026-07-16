//
// Created by Mathias Vatter on 16.07.25.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugar Format String
 */
class DesugarFormatString final : public ASTDesugaring {

public:
	explicit DesugarFormatString(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST *visit(NodeFormatString &node) override {
		for (auto &element : node.elements) {
			if (auto str = element->cast<NodeString>()) {
				// str->value = StringUtils::remove_single_quotes(str->value);
				// add remove_double_quotes for the builtin-preprocessor-tokens
				// str->value = node.quotes + StringUtils::remove_double_quotes(str->value) + node.quotes;
				str->value = "\"" + StringUtils::remove_double_quotes(str->value) + "\"";
			}
		}
		auto expr = NodeBinaryExpr::create_right_nested_binary_expr(node.elements, 0, token::STRING_OP);
		return node.replace_with(std::move(expr));
	}
};