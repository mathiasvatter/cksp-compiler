//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once


#include "ASTLowering.h"

/**
 * First Lowering Phase of num_elements
 * Checks for correct usage of num_elements with ndarrays and arrays (node second arg with arrays)
 * Filling in dimension 0 in case ndarray ref got called without dimension
 * Inserting clip function call for num member of NodeNumElements
 */
class LoweringNumElements : public ASTLowering {
private:
	std::string m_func_name = "Array"+OBJ_DELIMITER+"clip";
public:
	explicit LoweringNumElements(NodeProgram *program) : ASTLowering(program) {}


	NodeAST * visit(NodeNumElements &node) override {
		if(node.array->ty->get_type_kind() != TypeKind::Composite) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "<num_elements> can only be used with <Composite> types like <Arrays> or <NDArrays>.";
			error.exit();
		}

		if(node.array->get_node_type() == NodeType::ArrayRef and node.dimension) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "The <dimension> parameter of <num_elements> can not be used with <ArrayRef>.";
			error.exit();
		}

		if(node.dimension and node.dimension->get_node_type() == NodeType::Int) {
			auto int_node = static_cast<NodeInt*>(node.dimension.get());
			auto dim = int_node->value;
			if(node.array->get_node_type() == NodeType::NDArrayRef) {
				auto nd_array = static_cast<NodeNDArray *>(node.array->declaration);
				if (dim > nd_array->dimensions) {
					auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
					error.m_message =
						"Dimension " + std::to_string(dim) + " does not exist in <NDArray> " + node.array->name + ".";
					error.exit();
				} else if (dim <= 0) {
					auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
					error.m_message = "Dimension " + std::to_string(dim) + " is not valid. Dimensions start at 1. If"
																		   " you want to access the number of all elements, use 0.";
					error.exit();
				}
			}
		}

		// if no dimension is given and it is ndarray -> use 0
		if(node.array->get_node_type() == NodeType::NDArrayRef and !node.dimension) {
			// set default dimension to 0
			node.set_dimension(std::make_unique<NodeInt>(0, node.tok));
		}

		if(node.array->get_node_type() == NodeType::NDArrayRef) {
//			// get correct name (because local ndarrays get renamed) by using the lookup table in NodeProgram
//			if(!node.array->declaration) {
//				auto error = CompileError(ErrorType::InternalError, "", "", node.array->tok);
//				error.m_message = "Missing pointer to array definition: " + node.array->name;
//				error.exit();
//			}
//			auto it = m_program->num_element_constants.find(node.array->declaration);
//			if(it == m_program->num_element_constants.end()) {
//				auto error = CompileError(ErrorType::InternalError, "", "", node.array->tok);
//				error.m_message = "Missing num_elements constant for array: " + node.array->name;
//				error.exit();
//			}
//			// set found size_array
//			node.size_array = it->second;

			// add clip function when ndarray is used
			auto nd_array = static_cast<NodeNDArray*>(node.array->declaration);
			add_clip_function(m_program);
			node.set_dimension(get_clip_call(std::move(node.dimension), std::make_unique<NodeInt>(nd_array->dimensions, node.tok)));
		}

		return &node;
	}

	std::unique_ptr<NodeFunctionCall> get_clip_call(std::unique_ptr<NodeAST> x, std::unique_ptr<NodeAST> b) {
		auto clip_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				m_func_name,
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
	inline bool add_clip_function(NodeProgram* program) {
		// Prüfen, ob die Funktion bereits existiert
		auto it = program->function_lookup.find({m_func_name, 2});
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

		auto node_function = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				m_func_name,
				Token(),
				std::make_unique<NodeFunctionParam>(std::move(x)),
				std::make_unique<NodeFunctionParam>(std::move(b))
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(Token(), true),
			Token()
		);

		node_function->body->add_as_stmt(
			std::make_unique<NodeReturn>(
				Token(),
				std::move(final_expr)
			)
		);

		node_function->num_return_stmts = 1;
		node_function->num_return_params = 1;
		node_function->return_stmts.push_back(static_cast<NodeReturn*>(node_function->body->statements[0]->statement.get()));
		// Fügen Sie die neue Funktionsdefinition zum Programm hinzu
		program->additional_function_definitions.push_back(std::move(node_function));

		// Update function lookup so that the new function can be found
		program->update_function_lookup();

		return true;

	}

};