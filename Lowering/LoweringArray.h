//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * Determining the size of the array if possible
 * Throwing necessary errors if the array is not declared correctly
 * e.g. if size is not a constant or not able to be determined at compile time
 */
class LoweringArray : public ASTLowering {
private:
	bool size_is_constant = true;
public:
	explicit LoweringArray(NodeProgram* program) : ASTLowering(program) {}

	/// Determining array size at compile time -> not of references!
	void visit(NodeArray& node) override {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		if (node.parent->get_node_type() != NodeType::SingleDeclaration and
			node.parent->get_node_type() != NodeType::UIControl and !node.is_function_param()) {
			error.m_message = "Array is not a reference even though it is not part of a declaration.";
			error.m_got = node.name;
			error.exit();
		}
		if(node.is_function_param()) return;

		auto node_declaration = cast_node<NodeSingleDeclaration>(node.parent);
		// infer size from r_value param list
		if (!node.size) {
			// in case it is ui_control array and size is not determined
			if(!node_declaration) {
				error.m_message = "Unable to infer array size. Size of UI Control Array has to be determined at compile time.";
				error.exit();
			}
			// in case it is a declaration and declaration has no r-value -> throw error
			if (node_declaration and !node_declaration->value) {
				error.m_message =
					"Unable to infer array size. Size of Array has to be determined at compile time by providing an initializer list.";
				error.m_got = node.name;
				error.m_expected = "initializer list";
				error.exit();
			}
			if (auto param_list = cast_node<NodeParamList>(node_declaration->value.get())) {
				node.size = std::make_unique<NodeInt>((int32_t) param_list->params.size(), node.tok);
			}
		// array has size -> check if it is a constant variable
		} else {
			size_is_constant = true;
			node.size->accept(*this);
			if(!size_is_constant) {
				error.m_message = "Size of <Array> has to be a constant <Variable> or <Integer>.";
				error.m_got = node.size->get_string();
				error.exit();
			}
		}

		// check if assigned to sth then assignment has to be param list
//		if(node_declaration and node_declaration->value and node_declaration->value->get_node_type() != NodeType::ParamList) {
//			error.m_message = "Array has to be initialized with a list of values.";
//			error.m_got = node_declaration->value->get_string();
//			error.exit();
//		}

	}

	void visit(NodeVariableRef& node) override {
		if(node.data_type == DataType::Const and node.ty == TypeRegistry::Integer) {
			size_is_constant &= true;
		}
	}

	void visit(NodeReal& node) override {
		size_is_constant &= false;
	}

	void visit(NodeInt& node) override {
		size_is_constant &= true;
	}


};

