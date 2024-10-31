//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringGetControl : public ASTLowering {

public:
	explicit LoweringGetControl(NodeProgram* program) : ASTLowering(program) {}

//	NodeAST * visit(NodeSingleAssignment &node) override {
//		node.r_value->accept(*this);
//		// check if r_value is a NodeGetControl
//		if(node.l_value->get_node_type() != NodeType::GetControl) return &node;
//
//		auto get_control_statement = cast_node<NodeGetControl>(node.l_value.get());
//		// lower in case of array or nd array
//		get_control_statement->ui_id->lower(m_program);
//
//		std::string control_function = "set_control_par";
//
//		auto new_node = lowering(control_function, get_control_statement);
//		// add r_value as third parameter to set_control_par
//		new_node->function->add_arg(std::move(node.r_value));
//		return node.replace_with(std::move(new_node))->accept(*this);
//	};

	NodeAST * visit(NodeSetControl &node) override {
		// lower in case of array or nd array
		node.ui_id->lower(m_program);
		auto call = get_function_call("set_control_par", node.control_param, &node);
		call->function->prepend_arg(std::move(node.ui_id));
		call->function->add_arg(std::move(node.value));
		return node.replace_with(std::move(call));
	}

	NodeAST * visit(NodeGetControl &node) override {
		// lower in case of array or nd array
		node.ui_id->lower(m_program);
		auto call = get_function_call("get_control_par", node.control_param, &node);
		call->function->prepend_arg(std::move(node.ui_id));
		return node.replace_with(std::move(call));
	};

private:
	inline std::unique_ptr<NodeReference> get_full_control_param(const std::string& control_param) {
		std::string control_par = to_lower(control_param);
		if(control_par == "x") control_par = "pos_x";
		if(control_par == "y") control_par = "pos_y";
		if(control_par == "default") control_par += "_value";
		if(auto builtin_var = m_def_provider->get_builtin_variable(to_upper("control_par_"+control_par))) {
			return builtin_var->to_reference();
		}
		return nullptr;
	}

	inline std::unique_ptr<NodeFunctionCall> get_function_call(std::string control_function, std::string control_param, NodeAST* node) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		// get control_param from shorthand
		auto control_par = get_full_control_param(control_param);
		if(!control_par) {
			error.m_message = "Unknown control parameter: " + control_param;
			error.m_got = control_param;
			error.m_expected = "valid <control parameter> ($CONTROL_PAR...)";
			error.exit();
		}

		// determine if _str needs to be added to control function name
		if(node->ty == TypeRegistry::String) control_function += "_str";

		auto node_control_function = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				control_function,
				std::make_unique<NodeParamList>(node->tok, std::move(control_par)),
				node->tok
			),
			node->tok
		);
        node_control_function->function->ty = node->ty;
        node_control_function->kind = NodeFunctionCall::Kind::Builtin;
		return node_control_function;

	}

};