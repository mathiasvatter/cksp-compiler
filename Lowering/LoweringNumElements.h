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

		if(node.dimension) {
			// check if node is array ref -> no extra dimension allowed
			if(node.array->cast<NodeArrayRef>()) {
				auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
				error.m_message = "The <dimension> parameter of <num_elements> can not be used with <ArrayRef>.";
				error.exit();
			}
			// check if dimension is valid when int literal is used
			if(auto int_node = node.dimension->cast<NodeInt>()) {
				auto dim = int_node->value;
				if(auto nd_array = node.array->get_declaration()->cast<NodeNDArray>()) {
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
		}

		// when node is array_ref check if node has wildcard -> if yes, kill index
		if(auto node_array = node.array->cast<NodeArrayRef>()) {
			if(node_array->num_wildcards()) {
				node_array->index = nullptr;
			}
		} else if(auto node_ndarray = node.array->cast<NodeNDArrayRef>()) {
			auto nd_array = node.array->get_declaration()->cast<NodeNDArray>();
			if(!nd_array) {
				auto error = CompileError(ErrorType::VariableError, "", "", node.tok);
				error.m_message = "<NDArrayRef> has somehow a declaration that is not an <NDArray>.";
				error.exit();
			}
			// check if node is ndarray ref -> check for wildcard index notation and adjust dimension param accordingly
			handle_wildcard_notation(*node_ndarray, *nd_array, node);



			// add clip function when ndarray is used
			add_clip_function(m_program);
			node.set_dimension(get_clip_call(std::move(node.dimension), std::make_unique<NodeInt>(nd_array->dimensions, node.tok)));
		}

		return &node;
	}

private:

	static void handle_wildcard_notation(NodeNDArrayRef& array, const NodeNDArray& declaration, NodeNumElements& node) {
		auto tok = array.tok;
		int num_wildcards = array.num_wildcards();
		int num_dimensions = declaration.dimensions;

		// num_elements(ndarray[*, *], 1)
		if(num_wildcards and node.dimension) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
			error.m_message = "Wildcard notation in <NDArrayRef> does not allow for dimension parameter in <num_elements>.";
			error.exit();
		}

		// num_elements(ndarray[*, *]) -> num_elements(ndarray)
		if(num_wildcards == num_dimensions) {
			// set default dimension to 0
			array.indexes = nullptr;
		}

		// num_elements(ndarray) -> num_elements(ndarray, 0)
		// if no dimension is given and it is ndarray -> use 0
		if(num_wildcards == 0 and !node.dimension) {
			// set default dimension to 0
			node.set_dimension(std::make_unique<NodeInt>(0, node.tok));
			return;
		}

		if(num_wildcards == 0 and node.dimension) {
			return;
		}

		// num_elements(ndarray[*, 10]) -> num_elements(ndarray, 1) // wildcard position -> dimension
		if(num_wildcards == 1) {
			// get index of wildcard
			int idx = 0;
			for(int i = 0; i < array.indexes->params.size(); i++) {
				if(array.indexes->params[i]->cast<NodeWildcard>()) {
					idx = 0;
					break;
				}
			}
			// set dimension to index + 1
			idx++;
			array.indexes = nullptr;
			node.set_dimension(std::make_unique<NodeInt>(idx, tok));
			return;
		}

		// num_elements(ndarray[*, *, 2])
		if(num_wildcards > 1 and num_wildcards < num_dimensions) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
			error.m_message = "Cannot infer which dimension to return. Specify a single <Wildcard> (*).";
			error.exit();
		}

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

		auto x = std::make_shared<NodeVariable>(
			std::nullopt,
			"x",
			TypeRegistry::Integer,
			DataType::Mutable,
			Token()
		);
		auto b = std::make_shared<NodeVariable>(
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

		auto node_function = std::make_shared<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				m_func_name,
				Token(),
				std::make_unique<NodeFunctionParam>(std::move(x), nullptr, Token()),
				std::make_unique<NodeFunctionParam>(std::move(b), nullptr, Token())
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
		program->add_function_definition(node_function);
		return true;

	}

};