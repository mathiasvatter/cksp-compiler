//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "../ASTLowering.h"
#include "../../Desugaring/ASTDesugaring.h"

/// entry points: NodeSingleDeclaration
class DataLoweringNDArray : public ASTLowering {
private:

public:
	explicit DataLoweringNDArray(NodeProgram* program) : ASTLowering(program) {}

	/// Lowering of multidimensional arrays to arrays -> declaration
	/// Filling in num_elements param list member
	NodeAST* visit(NodeNDArray& node) override {
//        auto node_lowered_array = std::make_unique<NodeArray>(
//			node.persistence,
//			node.name,
//			TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1),
//			nullptr,
//			node.tok
//		);
		auto node_lowered_array = to_unique_ptr<NodeArray>(node.get_raw());

//		if(node.sizes) {
//			auto lowered_size_expr = NodeBinaryExpr::create_right_nested_binary_expr(node.sizes->params, 0, token::MULT);
//			lowered_size_expr->do_constant_folding();
//			node_lowered_array->set_size(lowered_size_expr->clone());
//			auto num_elements = std::move(node.sizes);
//			num_elements->prepend_param(std::move(lowered_size_expr));
//			node_lowered_array->set_num_elements(std::move(num_elements));
//		}
//		node_lowered_array->name = "_" + node_lowered_array->name;
//		node_lowered_array->match_metadata(node.get_shared());
//		node_lowered_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
		// call array lowering
		// node_lowered_array->lower(m_program);
		// node_lowered_array->num_elements->accept(*this);

		return node.replace_datastruct(std::move(node_lowered_array));
	}

    /// Lowering of multidimensional arrays to arrays when reference
	NodeAST* visit(NodeNDArrayRef& node) override {
        auto error = CompileError(ErrorType::VariableError, "", "", node.tok);
        if(node.indexes and node.sizes and node.indexes->size() != node.sizes->size()) {
            error.m_message = "Number of indices does not match number of dimensions: " + node.name;
            error.exit();
        }
        // convert index of multidimensional array
        std::unique_ptr<NodeAST> node_expression = nullptr;
		if(node.indexes) {
			// if no sizes -> ndarray is func param
			if(!node.sizes) {
				error.m_message = "Unable to infer array size. Size of <NDArrayRef> has to be determined at compile time.";
				error.exit();
			}
			node_expression = NodeBinaryExpr::calculate_index_expression(node.sizes->params, node.indexes->params, 0,node.tok);
		}
        auto node_lowered_array = node.to_array_ref(std::move(node_expression));
        node_lowered_array->name = "_" + node_lowered_array->name;
        node_lowered_array->ty = node.ty->get_element_type();
		// if no sizes -> ndarray is func param and needs to have another type dimension
		if(!node.indexes) {
			node_lowered_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1);
		}
		if(node_lowered_array->index) node_lowered_array->index->do_constant_folding();
        // return node.replace_reference(std::move(node_lowered_array));
		return node.replace_with(std::move(node_lowered_array));
    }
};

