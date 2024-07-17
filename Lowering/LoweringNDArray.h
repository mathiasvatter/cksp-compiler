//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"
#include "../Desugaring/ASTDesugaring.h"

/// entry points: NodeSingleDeclaration
class LoweringNDArray : public ASTLowering {
public:
	explicit LoweringNDArray(NodeProgram* program) : ASTLowering(program) {}

	void visit(NodeSingleAssignment &node) override {
		lowered_node = &node;
		if(node.l_value->get_node_type() == NodeType::NDArrayRef and node.r_value->get_node_type() == NodeType::ParamList) {
			auto nd_array_ref = static_cast<NodeNDArrayRef*>(node.l_value.get());
			auto param_list = static_cast<NodeParamList*>(node.r_value.get());
			param_list->flatten();
			// nda := ((1,2,3,4),(5,6,7,8))
			if(!nd_array_ref->indexes) {
				// lower left side to array to utilize array lowering later on
				nd_array_ref->accept(*this);
				return;
			}
			auto wildcard_dims = get_wildcard_dimensions(nd_array_ref);
			int num_wildcards = wildcard_dims.first != -1 ? wildcard_dims.second-wildcard_dims.first+1 : 0;
			if(num_wildcards > 1) {
				// ndarray2[0, *, *] := ((1,2,3), (1,2,3))
				if(param_list->params.size() > 1) {
					lowered_node = node.replace_with(get_ranged_array_init_from_list(nd_array_ref, param_list, wildcard_dims));
					return;
				}
			}
			// case ndarray[2, *] := (0)
			if(param_list->params.size() == 1) {
				add_ndarray_init_function_def(m_program, nd_array_ref, wildcard_dims);
				lowered_node = node.replace_with(get_ndarray_function_call(nd_array_ref,
																		   param_list->params.at(0).get(),
																		   wildcard_dims, "init"));
				lowered_node->accept(*this);
				return;
			// ndarray[2, *] := (2,3,4)
			} else if (param_list->params.size() > 1) {
				lowered_node = node.replace_with(get_array_init_from_list(nd_array_ref, param_list, wildcard_dims));
				lowered_node->accept(*this);
				return;
			}
		// copying ndarrays to part of ndarrays -> copying over ndarrays where both have no indexes is handled by lowering array
		// nd1[5, *, *]: int[][] := nd2: int[][]
		} else if(node.l_value->get_node_type() == NodeType::NDArrayRef and node.r_value->get_node_type() == NodeType::NDArrayRef) {
			auto nd_array_ref = static_cast<NodeNDArrayRef *>(node.l_value.get());
			auto nd_array_copy = static_cast<NodeNDArrayRef *>(node.r_value.get());
			if(nd_array_ref->num_wildcards() > 0) {
				// get wildcard dimensions
				auto wildcard_dims = get_wildcard_dimensions(nd_array_ref);
				add_ndarray_copy_function_def(m_program, nd_array_ref, wildcard_dims);
				lowered_node = node.replace_with(get_ndarray_function_call(nd_array_ref, nd_array_copy, wildcard_dims, "copy"));
				lowered_node->accept(*this);
				return;
			}
		} else if(node.l_value->get_node_type() == NodeType::NDArrayRef and node.r_value->get_node_type() == NodeType::ArrayRef) {
			auto nd_array_ref = static_cast<NodeNDArrayRef *>(node.l_value.get());
			auto array_ref = static_cast<NodeArrayRef *>(node.r_value.get());
			if(nd_array_ref->num_wildcards() > 0) {
				// get wildcard dimensions
				auto wildcard_dims = get_wildcard_dimensions(nd_array_ref);
				add_ndarray_copy_function_def(m_program, nd_array_ref, wildcard_dims);
				lowered_node = node.replace_with(get_ndarray_function_call(nd_array_ref, array_ref, wildcard_dims, "copy"));
				lowered_node->accept(*this);
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
				return;
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
		// if no sizes -> ndarray is func param and needs to have another type dimension
		if(!node.indexes) {
			node_lowered_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
		}
        node_lowered_array->parent = node.parent;
        node_lowered_array->declaration = node.declaration;
		node_lowered_array->update_parents(node.parent);
        lowered_node = node.replace_with(std::move(node_lowered_array));
    }

	void visit(NodeFor& node) override {
		node.body->accept(*this);
		if(auto desugaring = node.get_desugaring(m_program)) {
			node.accept(*desugaring);
			// move replacement to this visitor in case of nested for loops
			auto replacement = std::move(desugaring->replacement_node);
			node.replace_with(std::move(replacement));
		}
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
	/// ndarray[2, *] := (0) -> function returns position of *, returns 2:2
	/// ndarray[2, *, *] := (0) -> returns 2:3
	static inline std::pair<int, int> get_wildcard_dimensions(NodeNDArrayRef* node) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		std::vector<bool> wildcards;
		// get wildcard dimension
		int wildcard_start = -1;
		int wildcard_end = -1;
		for(int i = 0; i<node->indexes->params.size(); i++) {
			auto &idx = node->indexes->params[i];
			bool is_wildcard = idx->get_node_type() == NodeType::Wildcard;
			wildcards.push_back(is_wildcard);
			if(wildcards.size() > 2) {
				// (1,0,1....)
				if(wildcards[wildcards.size()-2] and !wildcards[wildcards.size()-1]) {
					error.m_message = "Wildcards must be contiguous in <NDArray> index when assigning list.";
					error.exit();
				}
			}
			if (is_wildcard) {
				if (wildcard_start == -1) {
					wildcard_start = i;
				}
				wildcard_end = i;
			}
		}
		if(wildcard_start == -1) {
			error.m_message = "No wildcard found in <NDArray> index when assigning list.";
			error.exit();
		}
		// if more than one wildcard and it is not right aligned:
		if(wildcard_end-wildcard_start > 1 and wildcard_end < wildcards.size()-1) {
			error.m_message = "Wildcards have to be right aligned in index list.";
			error.exit();
		}
		return {wildcard_start, wildcard_end};
	}

	/// ndarray.<init|copy>.<type>.<num_dimensions>.<dimension>(ndarray: type[][], value: type)
	inline std::unique_ptr<NodeFunctionCall> get_ndarray_function_call(NodeNDArrayRef* node, NodeAST* value, std::pair<int, int> wildcard_dims, std::string instruction) {
		auto func_name = get_func_name(instruction, node->ty, node->sizes->params.size(), wildcard_dims);

		auto params = std::make_unique<NodeParamList>(Token());
		for(auto & idx : node->indexes->params) {
			if(idx->get_node_type() != NodeType::Wildcard) {
				params->add_param(std::move(idx));
			}
		}
		// node ref without indexes
		auto node_ndarray_ref = clone_as<NodeNDArrayRef>(node);
		node_ndarray_ref->indexes = nullptr;
		params->prepend_param(value->clone());
		params->prepend_param(std::move(node_ndarray_ref));
		auto node_function_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::move(params),
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
 	 * // function ndarray.init.<type>.<num_dimensions>.<start_dim>.<end_dim>(ndarray: type[][], value: type)
	 * function ndarray.init.Integer.2.1.1(ndarray: int[][], value: type)
	 * 	declare local _iter: int
	 * 	for _iter := 0 to ndarray.SIZE_D1-1
	 * 		ndarray[_iter, 2] := value
	 * 	end for
	 * end function
	 */
	inline bool add_ndarray_init_function_def(NodeProgram* program, NodeNDArrayRef* node, std::pair<int, int> wildcard_dims) {
		auto func_name = get_func_name("init", node->ty, node->sizes->params.size(), wildcard_dims);
		// check if function with this type already exists
		if(program->function_lookup.find({func_name, wildcard_dims.first+2}) != program->function_lookup.end()) {
			return false;
		}
		auto node_nd_array_ref = clone_as<NodeNDArrayRef>(node);
		node_nd_array_ref->name = "ndarray";
		node_nd_array_ref->sizes = nullptr;
		// holds all iterators per wildcard
		std::vector<std::unique_ptr<NodeDataStructure>> iterators; int count = 1; int count2 = 1;
		std::vector<std::unique_ptr<NodeAST>> lower_bounds;
		std::vector<std::unique_ptr<NodeAST>> upper_bounds;
		auto params = std::make_unique<NodeParamList>(node->tok);
		for(auto & param : node_nd_array_ref->indexes->params) {
			if(param->get_node_type() == NodeType::Wildcard) {
				auto node_iterator = std::make_unique<NodeVariable>(std::nullopt, "_iter"+std::to_string(count), TypeRegistry::Integer, DataType::Mutable, Token());
				node_iterator->is_local = true;
				param = node_iterator->to_reference();
				iterators.push_back(std::move(node_iterator));
				auto node_upper_bound = std::make_unique<NodeVariableRef>(node_nd_array_ref->name + ".SIZE_D" + std::to_string(count), Token());
				node_upper_bound->kind = NodeReference::Compiler;
				upper_bounds.push_back(std::move(node_upper_bound));
				lower_bounds.push_back(std::make_unique<NodeInt>(0, Token()));
				count++;
			} else {
				auto node_iterate_dim = std::make_unique<NodeVariable>(std::nullopt, "param"+std::to_string(count2), TypeRegistry::Integer, DataType::Mutable, Token());
				count2++;
				param = node_iterate_dim->to_reference();
				params->add_param(std::move(node_iterate_dim));
			}
		}
		node_nd_array_ref->indexes->set_child_parents();

		auto node_value = std::make_unique<NodeVariable>(std::nullopt, "value", node->ty->get_element_type(), DataType::Mutable, Token());
		auto node_value_ref = node_value->to_reference();
		// create for loop
		auto node_body = std::make_unique<NodeBlock>(Token(), true);
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			node_nd_array_ref->clone(),
			std::move(node_value_ref),
			Token()
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), Token()));
		node_body->wrap_in_loop_nest(std::move(iterators), std::move(lower_bounds), std::move(upper_bounds));
		auto node_ndarray = std::make_unique<NodeNDArray>(std::nullopt,
														  "ndarray",
														  TypeRegistry::add_composite_type(CompoundKind::Array, node->ty->get_element_type(), (int)node->sizes->params.size()),
														  nullptr, Token()
		);
		// make function definition
		params->prepend_param(std::move(node_value));
		params->prepend_param(std::move(node_ndarray));
		auto node_function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::move(params),
				Token()
			),
			std::nullopt,
			false,
			std::move(node_body),
			Token()
		);

		// lower nd arrays to arrays
		node_function_def->accept(*this);
		program->additional_function_definitions.push_back(std::move(node_function_def));
		// update function lookup so that the new function can be found
		program->update_function_lookup();
		return true;
	}

	inline const std::string get_func_name(std::string instruction, Type* ty, int dimension, std::pair<int, int> wildcard_dims) const {
		return "ndarray."+instruction+"." + ty->get_element_type()->to_string() + ".dim" + std::to_string(dimension) + ".from" + std::to_string(wildcard_dims.first) + ".to" + std::to_string(wildcard_dims.second);
	}
	/**
	 * Pre Lowering:
	 * ndarray[2, *] := v        // ndarray := ((1,2,3), (0,0,0), (7,8,9))
	 * Post Lowering:
 	 * // function ndarray.copy.<type>.<num_dimensions>.<start_dim>.<end_dim>(ndarray: type[][], v: type[][])
	 * function ndarray.copy.Integer.2.1.2(ndarray: int[][], to_copy: int[][])
	 * 	declare local _iter1: int, local _iter2: int
	 * 	for _iter1 := 0 to ndarray.SIZE_D1-1
	 * 		for _iter2 := 0 to ndarray.SIZE_D2-1
	 * 			ndarray[_iter1, _iter2] := to_copy[_iter1, _iter2]
	 * 		end for
	 * 	end for
	 * end function
	 */
	inline bool add_ndarray_copy_function_def(NodeProgram* program, NodeNDArrayRef* node, std::pair<int, int> wildcard_dims) {
		auto func_name = get_func_name("copy", node->ty, node->sizes->params.size(), wildcard_dims);
		// check if function with this type already exists
		if(program->function_lookup.find({func_name, wildcard_dims.first+2}) != program->function_lookup.end()) {
			return false;
		}
		auto node_nd_array_ref = clone_as<NodeNDArrayRef>(node);
		node_nd_array_ref->name = "ndarray"; node_nd_array_ref->sizes = nullptr;
		// holds all iterators per wildcard
		std::vector<std::unique_ptr<NodeDataStructure>> iterators; int count = 1; int count2 = 1;
		std::vector<std::unique_ptr<NodeAST>> lower_bounds;
		std::vector<std::unique_ptr<NodeAST>> upper_bounds;
		auto params = std::make_unique<NodeParamList>(node->tok);
		for(auto & param : node_nd_array_ref->indexes->params) {
			if(param->get_node_type() == NodeType::Wildcard) {
				auto node_iterator = std::make_unique<NodeVariable>(std::nullopt, "_iter"+std::to_string(count), TypeRegistry::Integer, DataType::Mutable, Token());
				node_iterator->is_local = true;
				param = node_iterator->to_reference();
				iterators.push_back(std::move(node_iterator));
				auto node_upper_bound = std::make_unique<NodeVariableRef>(node_nd_array_ref->name + ".SIZE_D" + std::to_string(count), Token());
				node_upper_bound->kind = NodeReference::Compiler;
				upper_bounds.push_back(std::move(node_upper_bound));
				lower_bounds.push_back(std::make_unique<NodeInt>(0, Token()));
				count++;
			} else {
				auto node_iterate_dim = std::make_unique<NodeVariable>(std::nullopt, "param"+std::to_string(count2), TypeRegistry::Integer, DataType::Mutable, Token());
				count2++;
				param = node_iterate_dim->to_reference();
				params->add_param(std::move(node_iterate_dim));
			}
		}
		node_nd_array_ref->indexes->set_child_parents();

		auto node_nd_array_copy = std::make_unique<NodeNDArray>(std::nullopt,
														  "to_copy",
														  TypeRegistry::add_composite_type(CompoundKind::Array, node->ty->get_element_type(), (int)iterators.size()),
														  nullptr, Token()
		);
		auto node_nd_array_copy_ref = node_nd_array_copy->to_reference();
		auto to_copy_ref = static_cast<NodeNDArrayRef*>(node_nd_array_copy_ref.get());
		// get indexes of to_copy -> to_copy[_iter1, _iter2]
		to_copy_ref->indexes = std::make_unique<NodeParamList>(to_copy_ref->tok);
		for(auto & iterator : iterators) {
			to_copy_ref->indexes->add_param(iterator->to_reference());
		}

		// create for loop
		auto node_body = std::make_unique<NodeBlock>(Token(), true);
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			node_nd_array_ref->clone(),
			std::move(node_nd_array_copy_ref),
			Token()
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), Token()));
		node_body->wrap_in_loop_nest(std::move(iterators), std::move(lower_bounds), std::move(upper_bounds));
		auto node_ndarray = std::make_unique<NodeNDArray>(std::nullopt,
														  "ndarray",
														  TypeRegistry::add_composite_type(CompoundKind::Array, node->ty->get_element_type(), (int)node->sizes->params.size()),
														  nullptr, Token()
											);
		// make function definition
		params->prepend_param(std::move(node_nd_array_copy));
		params->prepend_param(std::move(node_ndarray));
		auto node_function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::move(params),
				Token()
			),
			std::nullopt,
			false,
			std::move(node_body),
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
	static std::unique_ptr<NodeBlock> get_array_init_from_list(NodeNDArrayRef* ndarray_ref, NodeParamList* param_list, std::pair<int, int> wildcard_dims) {
		auto node_block = std::make_unique<NodeBlock>(ndarray_ref->tok);
		for(int i = 0; i<param_list->params.size(); i++) {
			auto node_ndarray_ref = clone_as<NodeNDArrayRef>(ndarray_ref);
			node_ndarray_ref->indexes->params[wildcard_dims.first] = std::make_unique<NodeInt>(i, ndarray_ref->tok);
			node_ndarray_ref->indexes->params[wildcard_dims.first]->parent = node_ndarray_ref->indexes.get();
			auto node_assignment = std::make_unique<NodeSingleAssignment>(
				std::move(node_ndarray_ref),
				param_list->params[i]->clone(),
				ndarray_ref->tok
			);
			node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), ndarray_ref->tok));
		}
		return node_block;
	}
 	/// for when there are multiple wildcards
	static std::unique_ptr<NodeBlock> get_ranged_array_init_from_list(NodeNDArrayRef* ndarray_ref, NodeParamList* param_list, std::pair<int, int> wildcard_dims) {
		// get sizes and indices
		std::vector<std::unique_ptr<NodeAST>> sizes = std::move(ndarray_ref->sizes->params);
		std::vector<std::unique_ptr<NodeAST>> indices = std::move(ndarray_ref->indexes->params);
		indices.erase(indices.begin()+wildcard_dims.first+1, indices.begin()+wildcard_dims.second+1);
		auto node_expression = calculate_index_expression(sizes, indices, 0, ndarray_ref->tok);
		// calc index ends in additional addition.
		auto node_index_expr = std::move(static_cast<NodeBinaryExpr*>(node_expression.get())->left);

		auto node_block = std::make_unique<NodeBlock>(ndarray_ref->tok);
		for(int i = 0; i<param_list->params.size(); i++) {
			auto node_array_ref = ndarray_ref->to_array_ref(nullptr);
			node_array_ref->name = "_"+node_array_ref->name;
			auto node_index = std::make_unique<NodeBinaryExpr>(token::ADD, node_index_expr->clone(), std::make_unique<NodeInt>(i, ndarray_ref->tok), ndarray_ref->tok);
			node_index->parent = node_array_ref.get();
			node_array_ref->index = std::move(node_index);
			auto node_assignment = std::make_unique<NodeSingleAssignment>(
				std::move(node_array_ref),
				param_list->params[i]->clone(),
				ndarray_ref->tok
			);
			node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), ndarray_ref->tok));
		}
		return node_block;
	}

	inline bool check_initializer_list_correctness(NodeNDArrayRef* ref, NodeParamList* matrix) {
		// init list is correct in assignment when for ndarray[m, n, p] := (m-elements(n-elements(p-elements)))
		return true;
	}


};

