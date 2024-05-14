//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringGetControl : public ASTLowering {

public:
	explicit LoweringGetControl(DefinitionProvider* def_provider) : ASTLowering(def_provider) {}

	void visit(NodeSingleAssignStatement &node) override {
		node.assignee->accept(*this);
		// check if assignee is a NodeGetControlStatement
		if(node.array_variable->get_node_type() != NodeType::GetControlStatement) return;

		auto get_control_statement = cast_node<NodeGetControlStatement>(node.array_variable.get());
		std::string control_function = "set_control_par";

		auto new_node = lowering(control_function, get_control_statement);
		// add assignee as third parameter to set_control_par
		new_node->function->args->params.push_back(std::move(node.assignee));
		new_node->update_parents(node.parent);
		node.replace_with(std::move(new_node));
	};

	void visit(NodeGetControlStatement &node) override {
		std::string control_function = "get_control_par";
		auto new_node = lowering(control_function, &node);
		new_node->update_parents(node.parent);
		node.replace_with(std::move(new_node));
	};

	void visit(NodeFunctionCall &node) override {
		if(auto property_func = m_def_provider->get_property_function(node.function.get())) {
			if(node.function->args->params.size() < 2)
				CompileError(ErrorType::SyntaxError,"Found Property Function with insufficient amount of arguments.", node.tok.line, "At least 2 arguments", std::to_string(node.function->args->params.size()), node.tok.file).exit();
			auto node_body = inline_property_function(property_func, std::move(node.function));
			node_body->accept(*this);
			node.replace_with(std::move(node_body));
		}
	};

private:
	inline std::unique_ptr<NodeBody> inline_property_function(NodeFunctionHeader* property_function, std::unique_ptr<NodeFunctionHeader> function_header) {
		auto node_body = std::make_unique<NodeBody>(function_header->tok);
		for(int i = 1; i<function_header->args->params.size(); i++) {
			auto node_get_control = std::make_unique<NodeGetControlStatement>(
				function_header->args->params[0]->clone(),
				property_function->args->params[i]->get_string(),
				function_header->tok
				);
			auto node_assignment = std::make_unique<NodeSingleAssignStatement>(
				std::move(node_get_control),
				std::move(function_header->args->params[i]),
				function_header->tok
				);
			node_body->statements.push_back(
				std::make_unique<NodeStatement>(std::move(node_assignment), node_body->tok)
			);
		}
		node_body->update_parents(function_header->parent);
		return std::move(node_body);
	}

	inline std::unique_ptr<NodeFunctionCall> lowering(std::string control_function, NodeGetControlStatement* node) {
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
		ASTType control_function_type = get_control_function_type(node->control_param);
		if(control_function_type == ASTType::String) control_function += "_str";

		auto node_control_function = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				control_function,
				std::make_unique<NodeParamList>(node->tok),
				node->tok
			),
			node->tok
		);

		auto node_get_ui_id = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"get_ui_id",
				std::make_unique<NodeParamList>(node->tok),
				node->tok
			),
			node->tok
		);

		if(auto node_reference = cast_node<NodeReference>(node->ui_id.get())) {
			// do not wrap in get_ui_id when it is a ui_control array
            auto node_declaration = m_def_provider->get_declaration(node_reference);
            if(node_declaration) {
                m_def_provider->match_data_structure(node_reference, node_declaration);
            }
			if(node_declaration and node_reference->declaration->data_type != DataType::UI_Control) {
				node_control_function->function->args->params.push_back(std::move(node->ui_id));
			} else if(node_declaration and node_reference->declaration->data_type == DataType::UI_Control) {
				node_get_ui_id->function->args->params.push_back(std::move(node->ui_id));
				node_control_function->function->args->params.push_back(std::move(node_get_ui_id));
			} else {
				error.m_message = "Expected UI Control or UI Control Array in <control statement>.";
				error.m_got = node_reference->name;
				error.m_expected = "UI Control or UI Control Array";
				error.exit();
			}
			node_control_function->function->args->params.push_back(std::move(control_par));
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

	static inline ASTType get_control_function_type(const std::string& control_param) {
		std::string control_par = to_lower(control_param);
		static const std::unordered_set<std::string> str_substrings{"name", "path", "picture", "help", "identifier", "label", "text"};
		static const std::unordered_set<std::string> int_substrings{"state", "alignment", "pos", "shifting"};
		ASTType type = ASTType::Integer;
		for (auto const &substring : str_substrings) {
			if(contains(control_par, substring)) {
				type = ASTType::String;
				break;
			}
		}
		for (auto const &substring : int_substrings) {
			if(contains(control_par, substring)) {
				type = ASTType::Integer;
				break;
			}
		}
		return type;
	}

};