//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Determining the size of the array if possible
 * Throwing necessary errors if the array is not declared correctly
 * e.g. if size is not a constant or not able to be determined at compile time
 */
class DataLoweringArray final : public ASTLowering {
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
	explicit DataLoweringArray(NodeProgram* program) : ASTLowering(program) {}

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
			if (const auto init_list = cast_node<NodeInitializerList>(node_declaration->value.get())) {
				node.size = std::make_unique<NodeInt>((int32_t) init_list->size(), node.tok);
			}
		// array has size -> check if it is a constant variable
		} else {
			// m_size_is_constant = true;
			node.size->accept(*this);
			if(!node.size->is_constant()) {
				error.m_message = "Size of <Array> has to be a constant <Variable> or <Integer>.";
				error.m_got = node.size->get_string();
				error.exit();
			}
		}
		if (node_declaration) {
			check_size_against_initializer_list(*node_declaration);
		}

		return &node;
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		// check if func is 'num_elements' which returns constant and can be used as array size
		if(node.kind == NodeFunctionCall::Kind::Builtin and node.function->get_num_args() == 1) {
			if(node.function->name == "num_elements" or node.function->name == "get_ui_id") {
				if(node.function->get_arg(0)->get_string() == m_current_array->name) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
					error.m_message = "Size of <Array> cannot reference itself.";
					error.exit();
				}
				// m_size_is_constant &= true;
				// return &node;
			}
		}
		// m_size_is_constant &= false;
		return &node;
	}


};

