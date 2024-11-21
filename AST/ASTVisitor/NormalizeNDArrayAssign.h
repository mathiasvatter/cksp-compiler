//
// Created by Mathias Vatter on 07.08.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../Lowering/DataLowering/NDArrayCopyFunction.h"

class NormalizeNDArrayAssign: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
public:
	explicit NormalizeNDArrayAssign(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* visit(NodeSingleAssignment &node) override {
		node.l_value->accept(*this);
		node.r_value->accept(*this);

		if(node.l_value->get_node_type() == NodeType::NDArrayRef and node.r_value->get_node_type() == NodeType::InitializerList) {
			auto nd_array_ref = static_cast<NodeNDArrayRef*>(node.l_value.get());
			auto init_list = static_cast<NodeInitializerList*>(node.r_value.get());
			init_list->flatten();
			// nda := ((1,2,3,4),(5,6,7,8))
			if(nd_array_ref->num_wildcards() == nd_array_ref->sizes->size()) {
				// lower left side to array to utilize array lowering later on
				nd_array_ref->indexes = nullptr;
				return &node;
			}
			auto wildcard_dims = get_wildcard_dimensions(nd_array_ref);
			int num_wildcards = wildcard_dims.first != -1 ? wildcard_dims.second-wildcard_dims.first+1 : 0;
			if(num_wildcards > 1) {
				// ndarray2[0, *, *] := ((1,2,3), (1,2,3))
				if(init_list->size() > 1) {
					return node.replace_with(get_ranged_array_init_from_list(nd_array_ref, init_list, wildcard_dims));
				}
			}
			// case ndarray[2, *] := (0)
			if(init_list->size() == 1) {
				add_ndarray_init_function_def(m_program, nd_array_ref, wildcard_dims);
				auto lowered = node.replace_with(get_ndarray_init_function_call(nd_array_ref,
																				init_list->elem(0).get(),
																				wildcard_dims));
				return lowered;
				// ndarray[2, *] := (2,3,4)
			} else if (init_list->size() > 1) {
				auto lowered = node.replace_with(get_array_init_from_list(nd_array_ref, init_list, wildcard_dims));
				return lowered;
			}
		// copying ndarrays to part of ndarrays -> copying over ndarrays where both have no indexes is handled by lowering array
		// nd1[5, *, *]: int[][] := nd2: int[][]
		} else if(node.l_value->get_node_type() == NodeType::NDArrayRef and node.r_value->get_node_type() == NodeType::NDArrayRef) {
			auto nd_array_ref = static_cast<NodeNDArrayRef *>(node.l_value.get());
			auto nd_array_copy = static_cast<NodeNDArrayRef *>(node.r_value.get());

			if(nd_array_ref->num_wildcards() == 0 and nd_array_copy->num_wildcards() == 0) return &node;
			// try to lower to array assignment
//			if(nd_array_copy->num_wildcards() == 0) return &node;
			// check amount of wildcards
			if (nd_array_ref->num_wildcards() != nd_array_copy->num_wildcards()) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Wildcard dimensions do not match in <NDArray> assignment.";
				error.exit();
			}
			NDArrayCopyFunction nd_array_copy_function(m_program, nd_array_ref, nd_array_copy);
			return node.replace_with(nd_array_copy_function.generate_copy_function());

		} else if(node.l_value->get_node_type() == NodeType::NDArrayRef and node.r_value->get_node_type() == NodeType::ArrayRef) {
			auto nd_array_ref = static_cast<NodeNDArrayRef *>(node.l_value.get());
			auto array_ref = static_cast<NodeArrayRef *>(node.r_value.get());
			if(nd_array_ref->num_wildcards() > 0) {
				// get wildcard dimensions
				NDArrayCopyFunction nd_array_copy_function(m_program, nd_array_ref, array_ref);
				return node.replace_with(nd_array_copy_function.generate_copy_function());
			}
		} else if(node.l_value->get_node_type() == NodeType::ArrayRef and node.r_value->get_node_type() == NodeType::NDArrayRef) {
			auto array_ref = static_cast<NodeArrayRef *>(node.l_value.get());
			auto nd_array_ref = static_cast<NodeNDArrayRef *>(node.r_value.get());
			if(nd_array_ref->num_wildcards() > 0) {
				// get wildcard dimensions
				NDArrayCopyFunction nd_array_copy_function(m_program, array_ref, nd_array_ref);
				return node.replace_with(nd_array_copy_function.generate_copy_function());
			}
		}
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		// only add wildcards if it is l_value in assignment
		if(node.is_l_value() or node.is_r_value()) {
			node.add_wildcards();
		}
		return &node;
	}


private:
	/// Checks number of wildcards and wildcard position, returns position of wildcard
	/// ndarray[2, *] := (0) -> function returns position of *, returns 2:2
	/// ndarray[2, *, *] := (0) -> returns 2:3
	static inline std::pair<int, int> get_wildcard_dimensions(NodeNDArrayRef* node) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		std::vector<bool> wildcards;
		// get wildcard dimension
		int wildcard_start = -1;
		int wildcard_end = -1;
		if(!node->indexes) {
			return {0, 0};
		}
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

	/// ndarray.<init>.<type>.<num_dimensions>.<dimension>(ndarray: type[][], value: type)
	static inline std::unique_ptr<NodeFunctionCall> get_ndarray_init_function_call(NodeNDArrayRef* node, NodeAST* value, std::pair<int, int> wildcard_dims) {
		auto func_name = get_init_func_name(node->ty, node->sizes->params.size(), wildcard_dims);

		auto params = std::make_unique<NodeParamList>(Token());
		for(auto & idx : node->indexes->params) {
			if(idx->get_node_type() != NodeType::Wildcard) {
				params->add_param(std::move(idx));
			}
		}
		// node ref without indexes
		auto node_ndarray_ref = clone_as<NodeNDArrayRef>(node);
		node_ndarray_ref->indexes = nullptr;
		if(auto node_ndarray_value_ref = value->cast<NodeNDArrayRef>()) {
			node_ndarray_value_ref->indexes = nullptr;
		}
		params->prepend_param(value->clone());
		params->prepend_param(std::move(node_ndarray_ref));
		auto node_function_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
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
		auto func_name = get_init_func_name(node->ty, node->sizes->params.size(), wildcard_dims);
		// check if function with this type already exists
		if(program->function_lookup.find({func_name, wildcard_dims.first+2}) != program->function_lookup.end()) {
			return false;
		}
		auto node_nd_array_ref = clone_as<NodeNDArrayRef>(node);
		node_nd_array_ref->name = "ndarray";
		node_nd_array_ref->sizes = nullptr;
		// holds all iterators per wildcard
		std::vector<std::shared_ptr<NodeDataStructure>> iterators; int count = 1; int count2 = 1;
		std::vector<std::unique_ptr<NodeAST>> lower_bounds;
		std::vector<std::unique_ptr<NodeAST>> upper_bounds;
		auto func_header = std::make_unique<NodeFunctionHeader>(func_name, node->tok);
		for(auto & param : node_nd_array_ref->indexes->params) {
			if(param->get_node_type() == NodeType::Wildcard) {
				auto node_iterator = std::make_shared<NodeVariable>(std::nullopt, "_iter"+std::to_string(count), TypeRegistry::Integer, DataType::Mutable, node->tok);
				node_iterator->is_local = true;
				param = node_iterator->to_reference();
				iterators.push_back(std::move(node_iterator));
				auto node_upper_bound = std::make_unique<NodeVariableRef>(node_nd_array_ref->name + ".SIZE_D" + std::to_string(count), node->tok);
//				node_upper_bound->kind = NodeReference::Compiler;
				node_upper_bound->data_type = DataType::Const;
				upper_bounds.push_back(std::move(node_upper_bound));
				lower_bounds.push_back(std::make_unique<NodeInt>(0, node->tok));
				count++;
			} else {
				auto node_iterate_dim = std::make_unique<NodeVariable>(std::nullopt, "param"+std::to_string(count2), TypeRegistry::Integer, DataType::Mutable, node->tok);
				count2++;
				param = node_iterate_dim->to_reference();
				func_header->add_param(std::move(node_iterate_dim));
			}
		}
		node_nd_array_ref->indexes->set_child_parents();

		auto node_value = std::make_unique<NodeVariable>(std::nullopt, "value", node->ty->get_element_type(), DataType::Mutable, Token());
		auto node_value_ref = node_value->to_reference();
		// create for loop
		auto node_body = std::make_unique<NodeBlock>(node->tok, true);
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			clone_as<NodeReference>(node_nd_array_ref.get()),
			std::move(node_value_ref),
			node->tok
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), node->tok));
		node_body->wrap_in_loop_nest(iterators, std::move(lower_bounds), std::move(upper_bounds));
		auto node_ndarray = std::make_unique<NodeNDArray>(std::nullopt,
														  "ndarray",
														  TypeRegistry::add_composite_type(CompoundKind::Array, node->ty->get_element_type(), (int)node->sizes->params.size()),
														  nullptr, node->tok
		);
		// make function definition
		func_header->prepend_param(std::move(node_value));
		func_header->prepend_param(std::move(node_ndarray));
		auto node_function_def = std::make_shared<NodeFunctionDefinition>(
			std::move(func_header),
			std::nullopt,
			false,
			std::move(node_body),
			node->tok
		);

		// lower nd arrays to arrays
		node_function_def->accept(*this);
		node_function_def->header->create_function_type(TypeRegistry::Void);
		node_function_def->ty = TypeRegistry::Void;
//		program->additional_function_definitions.push_back(std::move(node_function_def));
		// update function lookup so that the new function can be found
		program->add_function_definition(node_function_def);
//		program->update_function_lookup();
		return true;
	}

	static inline std::string get_init_func_name(Type* ty, int dimension, std::pair<int, int> wildcard_dims) {
		std::string name = "ndarray$dim" + std::to_string(dimension);
		for(int i=wildcard_dims.first; i<wildcard_dims.second; i++) {
			name+="$"+std::to_string(i);
		}
		name += "<-init[" + ty->get_element_type()->to_string() + "]";
		return name;
	}

	/**
	 * Pre Lowering:
	 * ndarray[2, *] := (2,3,4)
	 * Post Lowering:
	 * ndarray[2, 0] := 2
	 * ndarray[2, 1] := 3
	 * ndarray[2, 2] := 4
	 */
	static std::unique_ptr<NodeBlock> get_array_init_from_list(NodeNDArrayRef* ndarray_ref, NodeInitializerList* init_list, std::pair<int, int> wildcard_dims) {
		auto node_block = std::make_unique<NodeBlock>(ndarray_ref->tok);
		for(int i = 0; i<init_list->size(); i++) {
			auto node_ndarray_ref = clone_as<NodeNDArrayRef>(ndarray_ref);
			node_ndarray_ref->indexes->param(wildcard_dims.first) = std::make_unique<NodeInt>(i, ndarray_ref->tok);
			node_ndarray_ref->indexes->param(wildcard_dims.first)->parent = node_ndarray_ref->indexes.get();
			auto node_assignment = std::make_unique<NodeSingleAssignment>(
				std::move(node_ndarray_ref),
				init_list->elem(i)->clone(),
				ndarray_ref->tok
			);
			node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), ndarray_ref->tok));
		}
		return node_block;
	}
	/// for when there are multiple wildcards
	static std::unique_ptr<NodeBlock> get_ranged_array_init_from_list(NodeNDArrayRef* ndarray_ref, NodeInitializerList* init_list, std::pair<int, int> wildcard_dims) {
		// get sizes and indices
		std::vector<std::unique_ptr<NodeAST>> sizes = std::move(ndarray_ref->sizes->params);
		std::vector<std::unique_ptr<NodeAST>> indices = std::move(ndarray_ref->indexes->params);
		indices.erase(indices.begin()+wildcard_dims.first+1, indices.begin()+wildcard_dims.second+1);
		auto node_expression = DataLoweringNDArray::calculate_index_expression(sizes, indices, 0, ndarray_ref->tok);
		// calc index ends in additional addition.
		auto node_index_expr = std::move(static_cast<NodeBinaryExpr*>(node_expression.get())->left);

		auto node_block = std::make_unique<NodeBlock>(ndarray_ref->tok);
		for(int i = 0; i<init_list->size(); i++) {
			auto node_array_ref = ndarray_ref->to_array_ref(nullptr);
			node_array_ref->name = "_"+node_array_ref->name;
			auto node_index = std::make_unique<NodeBinaryExpr>(token::ADD, node_index_expr->clone(), std::make_unique<NodeInt>(i, ndarray_ref->tok), ndarray_ref->tok);
			node_index->parent = node_array_ref.get();
			node_array_ref->index = std::move(node_index);
			auto node_assignment = std::make_unique<NodeSingleAssignment>(
				std::move(node_array_ref),
				init_list->elem(i)->clone(),
				ndarray_ref->tok
			);
			node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), ndarray_ref->tok));
		}
		return node_block;
	}

};