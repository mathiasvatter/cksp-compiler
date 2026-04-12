//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * Determining the size of the array if possible
 *  - when r_value is initializer list
 *  - when r_value is array ref
 *  - when r_value is ndarray re
 * Throwing necessary errors if the array is not declared correctly
 * e.g. if size is not a constant or not able to be determined at compile time
 *
 */
class LoweringArray final : public ASTLowering {
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
	explicit LoweringArray(NodeProgram* program) : ASTLowering(program) {}

	/// Determining array size at compile time -> not of references!
	NodeAST * visit(NodeArray& node) override {
		// m_size_is_constant = true;
		m_current_array = &node;
		// check if we are in a declaration or a declaration of a ui control
		const auto node_declaration = node.parent->cast<NodeSingleDeclaration>();
		const auto node_ui_control = node.parent->cast<NodeUIControl>();
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		if (!node_declaration and !node_ui_control and !node.is_function_param()) {
			error.m_message = "Array is not a reference even though it is not part of a declaration.";
			error.m_got = node.name;
			error.exit();
		}
		if(node.is_function_param()) return &node;

		// infer size from r_value param list
		if (!node.size) {
			// in case it is ui_control array and size is not determined
			if(node_ui_control) {
				error.m_message = "Unable to infer array size. Size of UI Control Array has to be determined at compile time.";
				error.exit();
			}
			// in case it is a declaration and declaration has no r-value -> throw error
			if (node_declaration and !node_declaration->value) {
				error.m_message =
					"Unable to infer array size. Size of Array has to be determined at compile time by providing an initializer list.";
				error.m_got = node.name;
				error.m_expected = "<Initializer List>";
				error.exit();
			}
			if (const auto init_list = node_declaration->value->cast<NodeInitializerList>()) {
				node.size = std::make_unique<NodeInt>((int32_t) init_list->size(), node.tok);
			// in case declare array: int[] := arr
			} else if (auto array_ref = node_declaration->value->cast<NodeArrayRef>()) {
				// get the size from decl of array_ref
				auto assign_del = array_ref->get_declaration();
				if (!assign_del) DefinitionProvider::internal_missing_declaration_error(*array_ref);
				auto ref = assign_del->to_reference();
				node.set_size(ref->get_size());
			} else if (auto ndarray_ref = node_declaration->value->cast<NodeNDArrayRef>()) {
				auto assign_del = ndarray_ref->get_declaration();
				if (!assign_del) DefinitionProvider::internal_missing_declaration_error(*ndarray_ref);
				node.set_size(ndarray_ref->get_size());
			}
		// array has size -> check if it is a constant variable
		} else {
			// m_size_is_constant = true;
			node.size->accept(*this);
			if(!node.size->is_constant()) {
				error.set_token(node.size->tok);
				error.m_message = "Size of <Array> has to be a constant expression with constant <Variables> and/or <Integers>.";
				error.m_got = node.size->get_string();
				error.exit();
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
	/// pre: 	declare arr2[num_elements(arr)]: int[] := arr[*]
	/// port:   declare arr2[num_elements(arr)]: int[]
	///         arr2 := arr[*]
	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (!node.value) return &node;

		if (node.value->cast<NodeArrayRef>() || node.value->cast<NodeNDArrayRef>()) {
			auto assignment = node.to_assign_stmt();
			auto new_declaration = std::make_unique<NodeSingleDeclaration>(
				std::move(node.variable), nullptr, node.tok);
			auto block = std::make_unique<NodeBlock>(node.tok);
			block->add_as_stmt(std::move(new_declaration));
			block->add_as_stmt(std::move(assignment));
			return node.replace_with(std::move(block));
		}

		return &node;
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		// check if array size references itself
		if(node.kind == NodeFunctionCall::Kind::Builtin and node.function->get_num_args() == 1) {
			if(node.function->name == "num_elements") {
				if(node.function->get_arg(0)->get_string() == m_current_array->name) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
					error.m_message = "Size of <Array> cannot reference itself.";
					error.exit();
				}
			}
		}
		return &node;
	}

	NodeAST * visit(NodeNumElements& node) override {
		node.array->accept(*this);
		if (node.array->name == m_current_array->name) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "Size of <Array> cannot reference itself.";
			error.exit();
		}
		if(node.dimension) node.dimension->accept(*this);
		return &node;
	}


};

