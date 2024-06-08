//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringGetControl : public ASTLowering {

public:
	explicit LoweringGetControl(DefinitionProvider* def_provider) : ASTLowering(def_provider) {}

	void visit(NodeSingleAssignment &node) override {
		node.assignee->accept(*this);
		// check if assignee is a NodeGetControl
		if(node.array_variable->get_node_type() != NodeType::GetControlStatement) return;

		auto get_control_statement = cast_node<NodeGetControl>(node.array_variable.get());
		std::string control_function = "set_control_par";

		auto new_node = lowering(control_function, get_control_statement);
		// add assignee as third parameter to set_control_par
		new_node->function->args->params.push_back(std::move(node.assignee));
		new_node->function->args->set_child_parents();
		node.replace_with(std::move(new_node));
	};

	void visit(NodeGetControl &node) override {
		std::string control_function = "get_control_par";
		auto new_node = lowering(control_function, &node);
		new_node->update_parents(node.parent);
		node.replace_with(std::move(new_node));
	};

private:
	inline std::unique_ptr<NodeFunctionCall> lowering(std::string control_function, NodeGetControl* node) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		// get control_param from shorthand
		auto control_par = shorthand_to_control_param(node->control_param);
		if(!control_par) {
			error.m_message = "Unknown control parameter: " + node->control_param;
			error.m_got = node->control_param;
			error.m_expected = "valid <control parameter> ($CONTROL_PAR...)";
			error.exit();
		}

		// determine if _str needs to be added to control function name
		Type* control_function_type = get_control_function_type(node->control_param);
		if(control_function_type == TypeRegistry::String) control_function += "_str";

		auto node_control_function = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				control_function,
				std::make_unique<NodeParamList>(node->tok),
				node->tok
			),
			node->tok
		);
        node_control_function->function->ty = control_function_type;
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
            if(auto lowering = node_control_function->get_lowering(m_def_provider)) {
                lowering->visit(*node_control_function);
            }
			return node_control_function;
		}
		return nullptr;
	}

	inline std::unique_ptr<NodeReference> shorthand_to_control_param(const std::string& shorthand) {
		std::string control_par = to_lower(shorthand);
		if(control_par == "x") control_par = "pos_x";
		if(control_par == "y") control_par = "pos_y";
		if(control_par == "default") control_par += "_value";
		auto &builtin_vars = m_def_provider->builtin_variables;
		auto it = builtin_vars.find(to_upper(control_par));
		if(it == builtin_vars.end()) it = builtin_vars.find(to_upper("control_par_"+control_par));

		if(it != builtin_vars.end()) {
			return clone_as<NodeVariable>(it->second.get())->to_reference();
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