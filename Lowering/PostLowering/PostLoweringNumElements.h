//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Second Lowering Phase of num_elements
 * has to happen after function inlining
 * if dimension is given -> call to ndarray size array
 * if no dimension is given -> was array -> call to num_elements
 */
class PostLoweringNumElements : public ASTLowering {
private:

public:
	explicit PostLoweringNumElements(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeNumElements &node) override {
		if(node.array->ty->get_type_kind() != TypeKind::Composite) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "<num_elements> can only be used with <Composite> types like <Arrays> or <NDArrays>.";
			error.exit();
		}

		if(node.dimension) {
//			if(!node.size_array) {
//				auto error = CompileError(ErrorType::InternalError, "", "", node.array->tok);
//				error.m_message = "Missing num_elements constant for array: " + node.array->name;
//				error.exit();
//			}

			auto array_call = std::make_unique<NodeArrayRef>(
				node.array->sanitize_name()+OBJ_DELIMITER+"num_elements",
				std::move(node.dimension),
				node.tok
			);

//			auto array_call = std::make_unique<NodeArrayRef>(
//				node.size_array->name,
//				std::move(node.dimension),
//				node.tok
//			);
//			array_call->match_data_structure(node.size_array.get());
			array_call->match_data_structure(node.array->get_declaration());
			array_call->ty = TypeRegistry::Integer;
			return node.replace_with(std::move(array_call));
		} else {
			// no dimension -> use builtin num_elements function
			return node.replace_with(DefinitionProvider::num_elements(std::move(node.array)));
		}

	}

	inline static std::unique_ptr<NodeAST> inline_clip_function(std::unique_ptr<NodeAST> x, std::unique_ptr<NodeAST> b) {
		// b-x
		auto b_minus_x = std::make_unique<NodeBinaryExpr>(
			token::SUB,
			b->clone(),
			x->clone(),
			Token()
		);
		// sh_right(b-x, 31)
		auto sh_right_b_minus_x = DefinitionProvider::sh_right(std::move(b_minus_x), std::make_unique<NodeInt>(31, Token()));

		// (x-b) .and. sh_right((b-x), 31)
		auto and_expr = std::make_unique<NodeBinaryExpr>(
			token::BIT_AND,
			std::make_unique<NodeBinaryExpr>(
				token::SUB,
				x->clone(),
				b->clone(),
				Token()
			),
			std::move(sh_right_b_minus_x),
			Token()
		);

		// -x .and. sh_right(x, 31)
		auto sh_right_x = DefinitionProvider::sh_right(x->clone(), std::make_unique<NodeInt>(31, Token()));
		auto and_expr2 = std::make_unique<NodeBinaryExpr>(
			token::BIT_AND,
			std::make_unique<NodeUnaryExpr>(
				token::SUB,
				x->clone(),
				Token()
			),
			std::move(sh_right_x),
			Token()
		);

		// -x .and. sh_right(x, 31) + (x-b) .and. sh_right((b-x), 31)
		auto add_expr = std::make_unique<NodeBinaryExpr>(
			token::ADD,
			std::move(and_expr2),
			std::move(and_expr),
			Token()
		);

		// x + (-x .and. sh_right(x, 31)) - ((x - b) .and. sh_right((b - x), 31))
		return std::make_unique<NodeBinaryExpr>(
			token::ADD,
			x->clone(),
			std::move(add_expr),
			Token()
		);
	}


};