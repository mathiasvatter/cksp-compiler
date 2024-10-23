//
// Created by Mathias Vatter on 23.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugars BinaryExpression Operators to vanilla KSP builtin math functions
 * e.g.
 * 4 >> 3 -> sh_right(4, 3)
 * 1 << 3 -> sh_left(1, 3)
 * 3 ** 3 -> pow(2, 3)
 */
class DesugarBinaryExpr : public ASTDesugaring {
private:
	inline static std::unordered_map<token, std::string> operator_to_function {
		{token::SHIFT_RIGHT, "sh_right"},
		{token::SHIFT_LEFT, "sh_left"},
		{token::EXP, "pow"},
	};
public:
	explicit DesugarBinaryExpr(NodeProgram* program) : ASTDesugaring(program) {};

	inline NodeAST * visit(NodeBinaryExpr& node) override {
		node.left ->accept(*this);
		node.right->accept(*this);

		auto it = operator_to_function.find(node.op);
		if(it == operator_to_function.end()) {
			return &node;
		}
		auto desugared_op = it->second;

		auto call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionVarRef>(
				std::make_unique<NodeFunctionHeader>(
					desugared_op,
					std::make_unique<NodeParamList>(
						node.tok,
						std::move(node.left),
						std::move(node.right)
					),
					node.tok
				),
				node.tok
			),
			node.tok
		);

		return node.replace_with(std::move(call));
	}

};
