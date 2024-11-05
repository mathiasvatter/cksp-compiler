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
	NodeArray* m_current_array = nullptr;
	bool m_size_is_constant = true;

	static inline bool check_size_against_initializer_list(const NodeSingleDeclaration& node) {
		if(node.variable->get_node_type() == NodeType::Array) {
			auto node_array = static_cast<NodeArray*>(node.variable.get());
			// check if size of array is correct when given initializer list
			if(node.value and node.value->get_node_type() == NodeType::InitializerList) {
				auto init_list = static_cast<NodeInitializerList*>(node.value.get());
				if(node_array->size->get_node_type() == NodeType::Int) {
					auto node_int = static_cast<NodeInt*>(node_array->size.get());
					auto dims = init_list->get_dimensions();
					if(dims.size() != 1) {
						auto error = CompileError(ErrorType::SyntaxError, "", "", node_array->tok);
						error.m_message = "Initializer List for <Array> has to be one-dimensional.";
						error.exit();
						return false;
					}
					if(node_int->value < dims[0]) {
						auto error = CompileError(ErrorType::SyntaxError, "", "", node_array->tok);
						error.m_message = "Size of <Array> does not match the size of the initializer list.";
						error.m_got = std::to_string(init_list->size());
						error.m_expected = std::to_string(node_int->value);
						error.exit();
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

	NodeAST * visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);

		check_size_against_initializer_list(node);
		// add size constant ".SIZE" to every array declaration
		if(node.variable->get_node_type() == NodeType::Array) {
			auto node_array = static_cast<NodeArray*>(node.variable.get());
			auto node_body = std::make_unique<NodeBlock>(node.tok);
//			auto node_var = std::make_unique<NodeVariable>(
//				std::nullopt,
//				node_array->name + ".SIZE",
//				TypeRegistry::Integer,
//				DataType::Const, node.tok);
//			auto node_declaration = std::make_unique<NodeSingleDeclaration>(
//				std::move(node_var),
//				node_array->size->clone(), node.tok);
//			node_body->add_as_stmt(std::move(node_declaration));
			node_body->add_as_stmt(std::make_unique<NodeSingleDeclaration>(node.variable, std::move(node.value), node.tok));
			return node.replace_with(std::move(node_body));
		}
		return &node;
	}

	/// Determining array size at compile time -> not of references!
	NodeAST * visit(NodeArray& node) override {
		m_size_is_constant = true;
		m_current_array = &node;
		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		if (node.parent->get_node_type() != NodeType::SingleDeclaration and
			node.parent->get_node_type() != NodeType::UIControl and !node.is_function_param()) {
			error.m_message = "Array is not a reference even though it is not part of a declaration.";
			error.m_got = node.name;
			error.exit();
		}
		if(node.is_function_param()) return &node;

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
				error.m_expected = "<Initializer List>";
				error.exit();
			}
			if (auto init_list = cast_node<NodeInitializerList>(node_declaration->value.get())) {
				node.size = std::make_unique<NodeInt>((int32_t) init_list->size(), node.tok);
			}
		// array has size -> check if it is a constant variable
		} else {
			m_size_is_constant = true;
			node.size->accept(*this);
			if(!m_size_is_constant) {
				error.m_message = "Size of <Array> has to be a constant <Variable> or <Integer>.";
				error.m_got = node.size->get_string();
				error.exit();
			}
		}

		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		if(node.data_type == DataType::Const and node.ty == TypeRegistry::Integer) {
			m_size_is_constant &= true;
		}
		return &node;
	}

	NodeAST * visit(NodeReal& node) override {
		m_size_is_constant &= false;
		return &node;
	}

	NodeAST * visit(NodeInt& node) override {
		m_size_is_constant &= true;
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
				m_size_is_constant &= true;
				return &node;
			}
		}
		m_size_is_constant &= false;
		return &node;
	}


};

