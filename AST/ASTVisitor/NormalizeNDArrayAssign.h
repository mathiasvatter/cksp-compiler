//
// Created by Mathias Vatter on 07.08.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../Lowering/DataLowering/NDArrayCopyFunction.h"

class NormalizeNDArrayAssign final : public ASTVisitor {
	DefinitionProvider *m_def_provider;
public:
	explicit NormalizeNDArrayAssign(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* visit(NodeNumElements& node) override {
		node.array->accept(*this);
		if(node.dimension) node.dimension->accept(*this);
		return node.lower(m_program);
	}

	NodeAST* visit(NodeSortSearch& node) override {
		node.array->accept(*this);
		node.value->accept(*this);
		if(node.from) node.from->accept(*this);
		if(node.to) node.to->accept(*this);
		return node.lower(m_program);
	}

	NodeAST* visit(NodeSingleAssignment &node) override {
		node.l_value->accept(*this);
		node.r_value->accept(*this);

		if(auto nd_array_ref = node.l_value->cast<NodeNDArrayRef>()) {
			if(auto init_list = node.r_value->cast<NodeInitializerList>()) {
				init_list->flatten();
				// nda := ((1,2,3,4),(5,6,7,8))
				if(nd_array_ref->num_wildcards() == nd_array_ref->sizes->size()) {
					// lower left side to array to utilize array lowering later on
					nd_array_ref->indexes = nullptr;
					return &node;
				}
				auto wildcard_dims = nd_array_ref->get_wildcard_dimensions();
				int num_wildcards = wildcard_dims.first != -1 ? wildcard_dims.second-wildcard_dims.first+1 : 0;
				if(num_wildcards > 1) {
					// ndarray2[0, *, *] := ((1,2,3), (1,2,3))
					if(init_list->size() > 1) {
						return node.replace_with(get_ranged_array_init_from_list(nd_array_ref, init_list, wildcard_dims));
					}
				}
				// case ndarray[2, *] := (0)
				if(init_list->size() == 1) {
					auto block = ndarray_init_block(m_program, unique_ptr_cast<NodeNDArrayRef>(std::move(node.l_value)), std::move(init_list->elem(0)));
					auto lowered = node.replace_with(std::move(block));
					return lowered;
					// ndarray[2, *] := (2,3,4)
				} else if (init_list->size() > 1) {
					auto lowered = node.replace_with(get_array_init_from_list(nd_array_ref, init_list, wildcard_dims));
					return lowered;
				}
			// copying ndarrays to part of ndarrays -> copying over ndarrays where both have no indexes is handled by lowering array
			// nd1[5, *, *]: int[][] := nd2: int[][]
			} else if(auto nd_array_copy = node.r_value->cast<NodeNDArrayRef>()) {
				if(nd_array_ref->num_wildcards() == 0 and nd_array_copy->num_wildcards() == 0) return &node;
				// try to lower to array assignment
				// check amount of wildcards
				if (nd_array_ref->num_wildcards() != nd_array_copy->num_wildcards()) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
					error.m_message = "Wildcard dimensions do not match in <NDArray> assignment.";
					error.exit();
				}
				NDArrayCopyFunction nd_array_copy_function(m_program, nd_array_ref, nd_array_copy);
				return node.replace_with(nd_array_copy_function.generate_copy_body());

			} else if(auto array_ref = node.r_value->cast<NodeArrayRef>()) {
				if (nd_array_ref->num_wildcards() > 0) {
					// get wildcard dimensions
					NDArrayCopyFunction nd_array_copy_function(m_program, nd_array_ref, array_ref);
					return node.replace_with(nd_array_copy_function.generate_copy_body());
				}
			}
		}
		if(auto array_ref = node.l_value->cast<NodeArrayRef>()) {
			if(auto nd_array_ref = node.r_value->cast<NodeNDArrayRef>()) {
				if(nd_array_ref->num_wildcards() > 0) {
					// get wildcard dimensions
					NDArrayCopyFunction nd_array_copy_function(m_program, array_ref, nd_array_ref);
					return node.replace_with(nd_array_copy_function.generate_copy_body());
				}
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
	static std::unique_ptr<NodeBlock> ndarray_init_block(NodeProgram* program, std::unique_ptr<NodeNDArrayRef> node, std::unique_ptr<NodeAST> value) {
		// holds all iterators per wildcard
		std::vector<std::shared_ptr<NodeDataStructure>> iterators; int count = 1;
		std::vector<std::unique_ptr<NodeAST>> lower_bounds;
		std::vector<std::unique_ptr<NodeAST>> upper_bounds;
		for (size_t i = 0; i < node->indexes->params.size(); i++) {
			auto &param = node->indexes->params[i];
			if(param->get_node_type() == NodeType::Wildcard) {
				auto new_index = program->get_global_iterator(i);
				node->indexes->set_param(i, new_index->to_reference());
				iterators.push_back(std::move(new_index));
				auto num_elements = node->get_size(std::make_unique<NodeInt>(count, node->tok));
				upper_bounds.push_back(std::move(num_elements));
				lower_bounds.push_back(std::make_unique<NodeInt>(0, node->tok));

			}
			count++;
		}

		// create for loop
		auto node_body = std::make_unique<NodeBlock>(node->tok, true);
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			std::move(node),
			std::move(value),
			node->tok
		);
		node_body->add_as_stmt(std::move(node_assignment));
		node_body->wrap_in_loop_nest(iterators, std::move(lower_bounds), std::move(upper_bounds), false);

		node_body->collect_references();

		return node_body;
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
		auto node_expression = NodeBinaryExpr::calculate_index_expression(sizes, indices, 0, ndarray_ref->tok);
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