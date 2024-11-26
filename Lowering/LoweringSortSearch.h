//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * First Lowering Phase of search(ndarray[*,*], value) -> search(ndarray, value, from, to)
 * declare ndarray[10, 10] : int[][]
 * search(ndarray[0, *], 0)
 * search(ndarray[3, *, *], -1)
 * search(ndarray[*, 0], 0) // error, wildcard has to be in the last dimension
 * search(ndarray[*, *], 0) // -> search(ndarray, 0)
 * search(ndarray[10, *, 10, *], 0) // error, wildcard has to be in the last dimension
 * search(ndarray[0, *, *], 9, 3) // error, from and to parameters not allowed with wildcards
 */
class LoweringSortSearch : public ASTLowering {
private:
public:
	explicit LoweringSortSearch(NodeProgram *program) : ASTLowering(program) {}

	// handle wildcards and transform into from and to values
	// search(ndarray[3, *, *], -1)
	// from -> [3, 0, 0]
	// from = 3*dim1*dim2 + 0*dim2 + 0
	// to -> [3, num_elements(ndarray, 2)-1, num_elements(ndarray, 3)-1]
	// to = 3*dim1*dim2 + (dim2-1)*dim2 + (dim3-1)
	NodeAST * visit(NodeSortSearch &node) override {

		if(auto array_ref = node.array->cast<NodeArrayRef>()) {
			if(array_ref->index and array_ref->num_wildcards()) {
				array_ref->index = nullptr;
			}
			return &node;
		} else if(auto nd_array_ref = node.array->cast<NodeNDArrayRef>()) {
			if(node.from || node.to) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.array->tok);
				error.m_message = "Found <search> function with <NDArrayRef> and from/to parameters. "
								  "'from' and 'to' parameters are not allowed in this case. Use <Wildcards> instead to search specific dimensions. "
								  "Wildcard bounds are automatically derived.";
				error.exit();
			}
			// search(ndarray, -1) -> no wildcards
			if(!nd_array_ref->num_wildcards()) {
				return &node;
			}
			if(!nd_array_ref->sizes) {
				auto error = CompileError(ErrorType::InternalError, "", "", node.array->tok);
				error.m_message = "No sizes given for <NDArrayRef> in <search>.";
				error.exit();
			}
			if(!nd_array_ref->indexes) {
				auto error = CompileError(ErrorType::InternalError, "", "", node.array->tok);
				error.m_message = "No indexes given for <NDArrayRef> in <search>.";
				error.exit();
			}

			auto wildcard_dims = nd_array_ref->get_wildcard_dimensions();
			// check that they are right aligned
			if(wildcard_dims.second != nd_array_ref->indexes->size()-1) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.array->tok);
				error.m_message = "<Wildcards> have to be in the last dimension when using with <search>.";
				error.exit();
			}

			// get from param_list -> [3, *, *] -> [3, 0, 0]
			auto from_list = std::make_unique<NodeParamList>(node.tok);
			for(auto & index : nd_array_ref->indexes->params) {
				if(auto wildcard = index->cast<NodeWildcard>()) {
					from_list->add_param(std::make_unique<NodeInt>(0, node.tok));
				} else {
					from_list->add_param(index->clone());
				}
			}
			node.set_from(NodeBinaryExpr::calculate_index_expression(nd_array_ref->sizes->params, from_list->params, 0, node.tok));

			// get to param_list -> [3, *, *] -> [3, num_elements(ndarray, 2)-1, num_elements(ndarray, 3)-1]
			auto to_list = std::make_unique<NodeParamList>(node.tok);
			for(int i=0; i<nd_array_ref->indexes->size(); i++) {
				if(nd_array_ref->indexes->params[i]->cast<NodeWildcard>()) {
					auto num_elements = nd_array_ref->sizes->param(i)->clone();
					auto to = std::make_unique<NodeBinaryExpr>(
						token::SUB,
						std::move(num_elements),
						std::make_unique<NodeInt>(1, node.tok),
						node.tok
					);
					to_list->add_param(std::move(to));
				} else {
					to_list->add_param(nd_array_ref->indexes->params[i]->clone());
				}
			}
			node.set_to(NodeBinaryExpr::calculate_index_expression(nd_array_ref->sizes->params, to_list->params, 0, node.tok));
			if(node.from) node.from->do_constant_folding();
			if(node.to) node.to->do_constant_folding();
			nd_array_ref->indexes = nullptr;
			return &node;
		} else {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "First argument for function call <search> must be of <Composite> Type.";
			error.exit();
		}

		return &node;
	}


};