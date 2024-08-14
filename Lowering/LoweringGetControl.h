//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringGetControl : public ASTLowering {

public:
	explicit LoweringGetControl(NodeProgram* program) : ASTLowering(program) {}

	NodeAST * visit(NodeSingleAssignment &node) override {
		node.r_value->accept(*this);
		// check if r_value is a NodeGetControl
		if(node.l_value->get_node_type() != NodeType::GetControl) return &node;

		auto get_control_statement = cast_node<NodeGetControl>(node.l_value.get());
		// lower in case of array or nd array
		get_control_statement->ui_id->lower(m_program);

		std::string control_function = "set_control_par";
		auto new_node = lowering(control_function, get_control_statement);
		// add r_value as third parameter to set_control_par
		new_node->function->args->params.push_back(std::move(node.r_value));
		new_node->function->args->set_child_parents();
		return node.replace_with(std::move(new_node));
	};

	NodeAST * visit(NodeGetControl &node) override {
		// lower in case of array or nd array
		node.ui_id->lower(m_program);
		std::string control_function = "get_control_par";
		auto new_node = lowering(control_function, &node);
		new_node->update_parents(node.parent);
		return node.replace_with(std::move(new_node));
	};

private:
	inline std::unique_ptr<NodeFunctionCall> lowering(std::string control_function, NodeGetControl* node) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		// get control_param from shorthand
		auto control_par = node->get_full_control_param(m_def_provider);
		if(!control_par) {
			error.m_message = "Unknown control parameter: " + node->control_param;
			error.m_got = node->control_param;
			error.m_expected = "valid <control parameter> ($CONTROL_PAR...)";
			error.exit();
		}

		// determine if _str needs to be added to control function name
		if(node->ty == TypeRegistry::String) control_function += "_str";

		auto node_control_function = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				control_function,
				std::make_unique<NodeParamList>(node->tok),
				node->tok
			),
			node->tok
		);
        node_control_function->function->ty = node->ty;
        node_control_function->kind = NodeFunctionCall::Kind::Builtin;

		if(auto node_reference = cast_node<NodeReference>(node->ui_id.get())) {
			// do not wrap in get_ui_id when it is a ui_control array
            auto node_declaration = m_def_provider->get_declaration(node_reference);
            if(node_declaration) {
                node_reference->match_data_structure(node_declaration);
            }

            node_control_function->function->args->params.push_back(std::move(node->ui_id));
			node_control_function->function->args->params.push_back(std::move(control_par));
			node_control_function->function->args->set_child_parents();

            // check if var needs is ui control and needs to wrapped in get_ui_id
			node_control_function->lower(m_program);
			return node_control_function;
		}
		return nullptr;
	}

};