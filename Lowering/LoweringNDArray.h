//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"

/// entry points: NodeSingleDeclaration
class LoweringNDArray : public ASTLowering {
public:
	explicit LoweringNDArray(NodeProgram* program) : ASTLowering(program) {}

	void visit(NodeSingleAssignment &node) override {
		lowered_node = &node;
		if(node.l_value->get_node_type() == NodeType::NDArrayRef and node.r_value->get_node_type() == NodeType::ParamList) {
			auto nd_array_ref = static_cast<NodeNDArrayRef*>(node.l_value.get());
			auto param_list = static_cast<NodeParamList*>(node.r_value.get());
			if(!nd_array_ref->indexes) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "<NDArray> needs to have indices when in <Assign> Statement.";
				error.exit();
			}
			// case ndarray[2, *] := (0)
			if(param_list->params.size() == 1) {
				auto wildcard_dim = get_wildcard_dimension(nd_array_ref);
				add_ndarray_init_function_def(m_program, nd_array_ref, wildcard_dim);
				auto new_node = node.replace_with(get_ndarray_init_function_call(nd_array_ref, param_list->params.at(0).get(), wildcard_dim));
				new_node->accept(*this);
				return;
			// ndarray[2, *] := (2,3,4)
			} else if (param_list->params.size() > 1) {
				auto wildcard_dim = get_wildcard_dimension(nd_array_ref);
				auto new_node = node.replace_with(get_array_init_from_list(nd_array_ref, param_list, wildcard_dim));
				new_node->accept(*this);
				return;
			}
		}
		node.l_value->accept(*this);
		node.r_value->accept(*this);
	}

	/**
	 * returns a statement list with the declarations of the size constants of the array
	 * Pre Lowering of NDArray to Array
	 *  declare ndarray[3, 3]: int[][] := (1,2,3,4,5,6,7,8,9) // ndarray := ((1,2,3), (4,5,6), (7,8,9))
	 * Post Lowering:
	 *  declare const ndarray.SIZE_D1 := 3
	 * 	declare const ndarray.SIZE_D2 := 3
	 * 	declare _ndarray[3 * 3]: int[] := (1,2,3,4,5,6,7,8,9) // ndarray := ((1,2,3), (4,5,6), (7,8,9))
	 */
	void visit(NodeSingleDeclaration &node) override {
		lowered_node = &node;
        if(node.variable->get_node_type() == NodeType::NDArray) {
            if (auto node_ndarray = cast_node<NodeNDArray>(node.variable.get())) {
                auto node_body = std::make_unique<NodeBlock>(node.tok);
                for (int i = 0; i < node_ndarray->dimensions; i++) {
                    auto node_var = std::make_unique<NodeVariable>(
                            std::optional<Token>(),
                            node_ndarray->name + ".SIZE_D" + std::to_string(i + 1),
                            TypeRegistry::Integer,
                            DataType::Const, node.tok);
                    auto node_declaration = std::make_unique<NodeSingleDeclaration>(
                            std::move(node_var),
                            node_ndarray->sizes->params[i]->clone(), node.tok);
                    auto node_statement = std::make_unique<NodeStatement>(std::move(node_declaration), node.tok);
                    node_statement->update_parents(node_body.get());
                    node_body->statements.push_back(std::move(node_statement));
                }
                node.variable->accept(*this);
                node_body->statements.push_back(std::make_unique<NodeStatement>(node.clone(), node.tok));
                lowered_node = node.replace_with(std::move(node_body));
            }
        }
	}
	/// Lowering of multidimensional arrays to arrays -> declaration
	void visit(NodeNDArray& node) override {
		std::unique_ptr<NodeAST> node_expression = nullptr;
		if(node.sizes) node_expression = NodeBinaryExpr::create_right_nested_binary_expr(node.sizes->params, 0, token::MULT);
        auto node_lowered_array = std::make_unique<NodeArray>(
                node.persistence,
                node.name,
                TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1),
                std::move(node_expression), node.tok);
		node_lowered_array->name = "_" + node_lowered_array->name;
		node_lowered_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
		node_lowered_array->parent = node.parent;
		node_lowered_array->is_local = node.is_local;
		node_lowered_array->data_type = node.data_type;
		if(auto lowering = node_lowered_array->get_lowering(m_program)) {
			node_lowered_array->accept(*lowering);
		}
		lowered_node = node.replace_with(std::move(node_lowered_array));
	}

    /// Lowering of multidimensional arrays to arrays when reference
    void visit(NodeNDArrayRef& node) override {
        auto error = CompileError(ErrorType::Variable, "", "", node.tok);
        if(node.indexes and node.sizes and node.indexes->params.size() != node.sizes->params.size()) {
            error.m_message = "Number of indices does not match number of dimensions: " + node.name;
            error.exit();
        }
        // convert index of multidimensional array
        std::unique_ptr<NodeAST> node_expression = nullptr;
		if(node.indexes) {
			// if no sizes -> ndarray is func param
			if(!node.sizes) {
				auto sizes = std::make_unique<NodeParamList>(node.tok);
				for(int i = 0; i<node.indexes->params.size(); i++) {
					auto size_const = std::make_unique<NodeVariableRef>(node.name + ".SIZE_D" + std::to_string(i+1), node.tok);
					size_const->kind = NodeReference::Compiler;
					sizes->add_param(std::move(size_const));
				}
				node.sizes = std::move(sizes);
				node.sizes->parent = &node;
			}
			node_expression = calculate_index_expression(node.sizes->params, node.indexes->params, 0,node.tok);
		}
        auto node_lowered_array = std::make_unique<NodeArrayRef>(
                node.name,
                std::move(node_expression), node.tok);
        node_lowered_array->name = "_" + node_lowered_array->name;
        node_lowered_array->ty = node.ty;
        node_lowered_array->parent = node.parent;
        node_lowered_array->declaration = node.declaration;
		node_lowered_array->update_parents(node.parent);
        lowered_node = node.replace_with(std::move(node_lowered_array));
    }

private:

	static std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok) {
		// Basisfall: letztes Element in der Berechnung
		if (dimension == indices.size() - 1) {
			return indices[dimension]->clone();
		}
		// Produkt der Größen der nachfolgenden Dimensionen
		std::unique_ptr<NodeAST> size_product = sizes[dimension + 1]->clone();
		for (size_t i = dimension + 2; i < sizes.size(); ++i) {
			size_product = std::make_unique<NodeBinaryExpr>(token::MULT, std::move(size_product), sizes[i]->clone(), tok);
		}
		// Berechnung des aktuellen Teils der Formel
		std::unique_ptr<NodeAST> current_part = std::make_unique<NodeBinaryExpr>(
			token::MULT, indices[dimension]->clone(), std::move(size_product), tok);

		// Rekursiver Aufruf für den nächsten Teil der Formel
		std::unique_ptr<NodeAST> next_part = calculate_index_expression(sizes, indices, dimension + 1, tok);

		// Kombinieren des aktuellen Teils mit dem nächsten Teil
		return std::make_unique<NodeBinaryExpr>(token::ADD, std::move(current_part), std::move(next_part), tok);
	}

	/// Checks number of wildcards and wildcard position, returns position of wildcard
	/// ndarray[2, *] := (0) -> function returns position of *
	static inline int get_wildcard_dimension(NodeNDArrayRef* node) {
		// get wildcard dimension
		int wildcard_dim = -1;
		int num_wildcards = 0;
		for(int i = 0; i<node->indexes->params.size(); i++) {
			auto &idx = node->indexes->params[i];
			if(idx->get_node_type() == NodeType::Wildcard) {
				wildcard_dim = i+1;
				num_wildcards++;
			}
		}
		if(num_wildcards > 1) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
			error.m_message = "Only one wildcard allowed in <NDArray> index when assigning list.";
			error.exit();
		}
		if(num_wildcards == 0) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
			error.m_message = "No wildcard found in <NDArray> index when assigning list.";
			error.exit();
		}
		return wildcard_dim;
	}

	/// ndarray.init.<type>.<num_dimensions>.<dimension>(ndarray: type[][], value: type)
	static inline std::unique_ptr<NodeFunctionCall> get_ndarray_init_function_call(NodeNDArrayRef* node, NodeAST* value, int wildcard_dim) {
		std::string func_name = "ndarray.init." + node->ty->get_element_type()->to_string() + "." + std::to_string(node->sizes->params.size()) + "." + std::to_string(wildcard_dim);
		// node ref without indexes
		auto node_ndarray_ref = clone_as<NodeNDArrayRef>(node);
		node_ndarray_ref->indexes = nullptr;
		auto node_function_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeParamList>(
					node->tok,
					std::move(node_ndarray_ref),
					value->clone()
				),
				node->tok
			),
			node->tok
		);
		return node_function_call;
	}

	/**
	 * Pre Lowering:
	 * ndarray[2, *] := (0)        // ndarray := ((1,2,3), (0,0,0), (7,8,9))
	 * Post Lowering:
	 * // function ndarray.init.<type>.<num_dimensions>.<dimension>()
	 * function ndarray.init.Integer.2.1(ndarray: int[][], value: type)
	 * 	declare local _iter: int := 0
	 * 		while (_iter < ndarray.SIZE_D2)
	 * 			ndarray[2, _iter] := 0
	 * 			inc(_iter)
	 * 		end while
	 * end function
	 */
	inline bool add_ndarray_init_function_def(NodeProgram* program, NodeNDArrayRef* node, int wildcard_dim) {
		std::string func_name = "ndarray.init." + node->ty->get_element_type()->to_string() + "." + std::to_string(node->sizes->params.size()) + "." + std::to_string(wildcard_dim);
		// check if function with this type already exists
		auto it = program->function_lookup.find({func_name, 3});
		if(it != program->function_lookup.end()) {
			return false;
		}
		auto node_nd_array_ref = clone_as<NodeNDArrayRef>(node);
		node_nd_array_ref->name = "ndarray";
		node_nd_array_ref->sizes = nullptr;
		auto node_iterator = std::make_unique<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, Token());
		node_iterator->is_local = true;
		auto node_value = std::make_unique<NodeVariable>(std::nullopt, "value", node->ty->get_element_type(), DataType::Mutable, Token());
		auto node_value_ref = node_value->to_reference();
		auto node_iterator_ref = node_iterator->to_reference();
		for(auto & idx : node_nd_array_ref->indexes->params) {
			if(idx->get_node_type() == NodeType::Wildcard) {
				idx = std::move(node_iterator_ref->clone());
			}
		}
		node_nd_array_ref->indexes->set_child_parents();

		auto node_while_body = std::make_unique<NodeBlock>(Token());
		node_while_body->scope = true;
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			node_nd_array_ref->clone(),
			std::move(node_value_ref),
			Token()
		);
		auto node_inc = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"inc",
				std::make_unique<NodeParamList>(Token(), node_iterator_ref->clone()), Token()),
			Token()
		);

		node_while_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), Token()));
		node_while_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_inc), Token()));

		auto node_dim_const = std::make_unique<NodeVariableRef>(node_nd_array_ref->name + ".SIZE_D" + std::to_string(wildcard_dim), Token());
		node_dim_const->kind = NodeReference::Compiler;
		auto node_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(
				token::LESS_THAN,
				node_iterator_ref->clone(),
				std::move(node_dim_const),
				Token()
			),
			std::move(node_while_body),
			Token()
		);
		auto node_block = std::make_unique<NodeBlock>( Token());
		node_block->scope = true;
		auto node_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(node_iterator),
			std::make_unique<NodeInt>(0, Token()),
			Token()
		);
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_decl), Token()));
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_while), Token()));

		auto node_ndarray = std::make_unique<NodeNDArray>(std::nullopt,
														"ndarray",
														TypeRegistry::add_composite_type(CompoundKind::Array, node->ty->get_element_type(), (int)node->sizes->params.size()),
														nullptr, Token()
													);
		// make function definition
		auto node_function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeParamList>(Token(), std::move(node_ndarray), std::move(node_value)),
				Token()
				),
				std::nullopt,
				false,
				std::move(node_block),
				Token()
			);

		// lower nd arrays to arrays
		node_function_def->accept(*this);
		program->additional_function_definitions.push_back(std::move(node_function_def));
		// update function lookup so that the new function can be found
		program->update_function_lookup();
		return true;
	}

	/**
	 * Pre Lowering:
	 * ndarray[2, *] := (2,3,4)
	 * Post Lowering:
	 * ndarray[2, 0] := 2
	 * ndarray[2, 1] := 3
	 * ndarray[2, 2] := 4
	 */
	static std::unique_ptr<NodeBlock> get_array_init_from_list(NodeNDArrayRef* ndarray_ref, NodeParamList* param_list, int wildcard_dim) {
		auto node_block = std::make_unique<NodeBlock>(ndarray_ref->tok);
		for(int i = 0; i<param_list->params.size(); i++) {
			auto node_ndarray_ref = clone_as<NodeNDArrayRef>(ndarray_ref);
			node_ndarray_ref->indexes->params[wildcard_dim-1] = std::make_unique<NodeInt>(i, ndarray_ref->tok);
			node_ndarray_ref->indexes->params[wildcard_dim-1]->parent = node_ndarray_ref->indexes.get();
			auto node_assignment = std::make_unique<NodeSingleAssignment>(
				std::move(node_ndarray_ref),
				param_list->params[i]->clone(),
				ndarray_ref->tok
			);
			node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), ndarray_ref->tok));
		}
		return node_block;
	}
};

