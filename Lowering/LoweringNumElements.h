//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once


#include "ASTLowering.h"

class LoweringNumElements : public ASTLowering {
private:

public:
	explicit LoweringNumElements(NodeProgram *program) : ASTLowering(program) {}


	NodeAST * visit(NodeNumElements &node) override {
		if(node.ty->get_type_kind() != TypeKind::Composite) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "<num_elements> can only be used with <Composite> types like <Arrays> or <NDArrays>.";
			error.exit();
		}

		if(node.get_node_type() == NodeType::ArrayRef and node.dimension) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "The <dimension> parameter of <num_elements> can not be used with <ArrayRef>.";
			error.exit();
		}

		// if no dimension is given and it is ndarray -> use 0
		if(node.get_node_type() == NodeType::NDArrayRef and !node.dimension) {
			node.set_dimension(std::make_unique<NodeInt>(0, node.tok));
		}

		if(node.get_node_type() == NodeType::NDArrayRef) {
			auto nd_array = static_cast<NodeNDArrayRef*>(node.array.get());
			auto array_call = std::make_unique<NodeArrayRef>(
				nd_array->sanitize_name() + OBJ_DELIMITER + "num_elements",
				inline_clip_function(std::move(node.dimension), std::make_unique<NodeInt>(nd_array->sizes->size(), node.tok)),
				node.tok
			);
			array_call->data_type = DataType::Const;
			return node.replace_with(std::move(array_call));
		}

		return node.replace_with(DefinitionProvider::num_elements(std::move(node.array)));
	}

	static std::unique_ptr<NodeFunctionCall> get_clip_call(std::unique_ptr<NodeAST> x, std::unique_ptr<NodeAST> b) {
		auto clip_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				"Array"+OBJ_DELIMITER+"clip",
				std::make_unique<NodeParamList>(Token(), std::move(x), std::move(b)),
				Token()
			),
			Token()
		);
		return clip_call;
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

	/**
	 * function clip(x, b)
	 * 	return x + (-x .and. sh_right(x, 31)) - ((x - b) .and. sh_right((b - x), 31))
	 * end function
	 */
	inline static bool add_clip_function(NodeProgram* program) {
		std::string func_name = "Array"+OBJ_DELIMITER+"clip";
		// Prüfen, ob die Funktion bereits existiert
		auto it = program->function_lookup.find({func_name, 2});
		if (it != program->function_lookup.end()) {
			return false; // Funktion existiert bereits
		}

		auto x = std::make_unique<NodeVariable>(
			std::nullopt,
			"x",
			TypeRegistry::Integer,
			DataType::Mutable,
			Token()
		);
		auto b = std::make_unique<NodeVariable>(
			std::nullopt,
			"b",
			TypeRegistry::Integer,
			DataType::Mutable,
			Token()
		);
		auto node_function = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				"clip",
				Token(),
				std::make_unique<NodeFunctionParam>(std::move(x)),
				std::make_unique<NodeFunctionParam>(std::move(b))
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(Token(), true),
			Token()
		);
		// b-x
		auto b_minus_x = std::make_unique<NodeBinaryExpr>(
			token::SUB,
			b->to_reference(),
			x->to_reference(),
			Token()
		);
		// sh_right(b-x, 31)
		auto sh_right_b_minus_x = DefinitionProvider::sh_right(std::move(b_minus_x), std::make_unique<NodeInt>(31, Token()));

		// (x-b) .and. sh_right((b-x), 31)
		auto and_expr = std::make_unique<NodeBinaryExpr>(
			token::BIT_AND,
			std::make_unique<NodeBinaryExpr>(
				token::SUB,
				x->to_reference(),
				b->to_reference(),
				Token()
			),
			std::move(sh_right_b_minus_x),
			Token()
		);

		// -x .and. sh_right(x, 31)
		auto sh_right_x = DefinitionProvider::sh_right(x->to_reference(), std::make_unique<NodeInt>(31, Token()));
		auto and_expr2 = std::make_unique<NodeBinaryExpr>(
			token::BIT_AND,
			std::make_unique<NodeUnaryExpr>(
				token::SUB,
				x->to_reference(),
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
		auto final_expr = std::make_unique<NodeBinaryExpr>(
			token::ADD,
			x->to_reference(),
			std::move(add_expr),
			Token()
		);

		node_function->body->add_stmt(
			std::make_unique<NodeStatement>(
				std::make_unique<NodeReturn>(
					Token(),
					std::move(final_expr)
				),
				Token()
			)
		);

		// Fügen Sie die neue Funktionsdefinition zum Programm hinzu
		program->additional_function_definitions.push_back(std::move(node_function));

		// Update function lookup so that the new function can be found
		program->update_function_lookup();

		return true;

	}

};