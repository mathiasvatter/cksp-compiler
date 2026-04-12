//
// Created by Mathias Vatter on 12.04.26.
//

#pragma once

#include "ASTLowering.h"

/**
 * Determining the size of the ndarray if possible
 *  - when r_value is initializer list
 *  - when r_value is array ref
 *  - when r_value is ndarray ref
 */
class LoweringNDArray final : public ASTLowering {
	NodeArray* m_current_array = nullptr;
	bool m_size_is_constant = true;

	static bool check_size_against_initializer_list(const NodeSingleDeclaration& node) {
		if(auto node_array = node.variable->cast<NodeArray>()) {
			if (!node.value) return false;
			// check if size of array is correct when given initializer list
			if(auto init_list = node.value->cast<NodeInitializerList>()) {
				node_array->size->do_constant_folding();
				if(auto node_int = node_array->size->cast<NodeInt>()) {
					auto dims = init_list->get_dimensions();
					if(dims.size() != 1) {
						auto error = CompileError(ErrorType::SyntaxError, "", "", node_array->tok);
						error.m_message = "Initializer List for <Array> has to be one-dimensional.";
						error.exit();
						return false;
					}
					if(node_int->value < dims[0]) {
						auto error = CompileError(ErrorType::SyntaxError, "", "", node_array->tok);
						error.m_message = "Size of <Array> does not match the size of the initializer list. Kontakt will ignore out of range initializers.";
						error.m_got = std::to_string(init_list->size());
						error.m_expected = std::to_string(node_int->value);
						error.print();
						return false;
					}
				} else {
					return true;
				}
			}
		}
		return false;
	}

public:
	explicit LoweringNDArray(NodeProgram* program) : ASTLowering(program) {}

	/// Determining array size at compile time -> not of references!
	NodeAST * visit(NodeNDArray& node) override {
		// check if we are in a declaration or a declaration of a ui control
		const auto node_declaration = node.parent->cast<NodeSingleDeclaration>();
		const auto node_ui_control = node.parent->cast<NodeUIControl>();
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		if (!node_declaration and !node_ui_control and !node.is_function_param()) {
			error.m_message = "NDArray is not a reference even though it is not part of a declaration.";
			error.m_got = node.name;
			error.exit();
		}
		if(node.is_function_param()) return &node;

		// infer size from r_value param list
		if (!node.sizes) {
			// in case it is ui_control array and size is not determined
			if(node_ui_control) {
				error.m_message = "Unable to infer array size. Size of multidimensional UI Control Array has to be determined at compile time.";
				error.exit();
			}
			// in case it is a declaration and declaration has no r-value -> throw error
			if (node_declaration and !node_declaration->value) {
				error.m_message =
					"Unable to infer array size. Size of NDArray has to be determined at compile time by providing an initializer list.";
				error.m_got = node.name;
				error.m_expected = "<Initializer List>";
				error.exit();
			}
			// 	declare ndarr: int[][] := [[1,2,3],[4,5, 4]]
			if (const auto init_list = node_declaration->value->cast<NodeInitializerList>()) {
				auto dims = init_list->get_dimensions();
				auto node_dims = std::make_unique<NodeParamList>(node.tok);
				for (auto dim : dims) {
					node_dims->add_param(std::make_unique<NodeInt>(dim, node.tok));
				}
				node.set_size(std::move(node_dims));
			// in case declare narr: int[][] := arr -> error!
			// } else if (auto array_ref = node_declaration->value->cast<NodeArrayRef>()) {
			// 	// get the size from decl of array_ref
			// 	auto assign_del = array_ref->get_declaration();
			// 	if (!assign_del) DefinitionProvider::internal_missing_declaration_error(*array_ref);
			// 	auto ref = assign_del->to_reference();
			// 	node.set_size(ref->get_size());
			// } else if (auto ndarray_ref = node_declaration->value->cast<NodeNDArrayRef>()) {
			// 	auto assign_del = ndarray_ref->get_declaration();
			// 	if (!assign_del) DefinitionProvider::internal_missing_declaration_error(*ndarray_ref);
			// 	node.set_size(ndarray_ref->get_size());
			}
		}
		if (node_declaration) {
			check_size_against_initializer_list(*node_declaration);
		}

		return &node;
	}

	/// if we have a declaration as entry point in CollectLowerings we enter this after we already entered
	/// visit NodeArray. If r_value is array or ndarray ref -> need to split up to assignment to
	/// benefit from NormalizeNDArrayAssign and NormalizeArrayAssign
	/// pre: 	declare ndarr2[1,2]: int[][] := ndarr3
	/// port:   declare ndarr2[1,2]: int[][]
	///         ndarr2 := ndarr3
	// NodeAST* visit(NodeSingleDeclaration& node) override {
	// 	if (!node.value) return &node;
	//
	// 	if (node.value->cast<NodeArrayRef>() || node.value->cast<NodeNDArrayRef>()) {
	// 		auto assignment = node.to_assign_stmt();
	// 		auto new_declaration = std::make_unique<NodeSingleDeclaration>(
	// 			std::move(node.variable), nullptr, node.tok);
	// 		auto block = std::make_unique<NodeBlock>(node.tok);
	// 		block->add_as_stmt(std::move(new_declaration));
	// 		block->add_as_stmt(std::move(assignment));
	// 		return node.replace_with(std::move(block));
	// 	}
	//
	// 	return &node;
	// }


};

