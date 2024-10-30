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
		new_node->function->add_arg(std::move(node.r_value));
		return node.replace_with(std::move(new_node))->accept(*this);
	};

	NodeAST * visit(NodeGetControl &node) override {
		// lower in case of array or nd array
		node.ui_id->lower(m_program);
		std::string control_function = "get_control_par";
		auto new_node = lowering(control_function, &node);
		new_node->update_parents(node.parent);
		return node.replace_with(std::move(new_node))->accept(*this);
	};

private:
	inline std::unique_ptr<NodeReference> get_full_control_param(NodeGetControl* node) {
		std::string control_par = to_lower(node->control_param);
		if(control_par == "x") control_par = "pos_x";
		if(control_par == "y") control_par = "pos_y";
		if(control_par == "default") control_par += "_value";
		if(auto builtin_var = m_def_provider->get_builtin_variable(to_upper("control_par_"+control_par))) {
			return builtin_var->to_reference();
		}
		return nullptr;
	}

	inline std::unique_ptr<NodeFunctionCall> lowering(std::string control_function, NodeGetControl* node) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		// get control_param from shorthand
		auto control_par = get_full_control_param(node);
		if(!control_par) {
			error.m_message = "Unknown control parameter: " + node->control_param;
			error.m_got = node->control_param;
			error.m_expected = "valid <control parameter> ($CONTROL_PAR...)";
			error.exit();
		}
		node->ty = get_control_function_type(control_par->name);

		// determine if _str needs to be added to control function name
		if(node->ty == TypeRegistry::String) control_function += "_str";

		auto node_control_function = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				control_function,
				std::make_unique<NodeParamList>(node->tok),
				node->tok
			),
			node->tok
		);
        node_control_function->function->ty = node->ty;
        node_control_function->kind = NodeFunctionCall::Kind::Builtin;

		if(auto reference = cast_node<NodeReference>(node->ui_id.get())) {
			// check declaration of ui_id to decide if it needs to be wrapped in get_ui_id later on in LoweringFunctionCall
			auto declaration = m_def_provider->get_declaration(reference);
			if(declaration) {
				reference->match_data_structure(declaration);
				reference->ty = declaration->ty;
			}

            node_control_function->function->add_arg(std::move(node->ui_id));
			node_control_function->function->add_arg(std::move(control_par));

            // check if var needs is ui control and needs to wrapped in get_ui_id
			node_control_function->lower(m_program);
			return node_control_function;
		}
		return nullptr;
	}

	static inline Type* get_control_function_type(const std::string& control_param) {
		std::string control_par = to_lower(control_param);
		static const std::unordered_set<std::string> str_substrings{"name", "path", "picture", "help", "identifier", "label", "text"};
		static const std::unordered_set<std::string> int_substrings{"state", "alignment", "pos", "shifting"};
		Type* type = TypeRegistry::Integer;
		for (auto const &substring : str_substrings) {
			if(contains(control_par, substring)) {
				type = TypeRegistry::String;
				break;
			}
		}
		for (auto const &substring : int_substrings) {
			if(contains(control_par, substring)) {
				type = TypeRegistry::Integer;
				break;
			}
		}
		return type;
	}

};