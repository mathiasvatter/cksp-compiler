//
// Created by Mathias Vatter on 02.01.24.
//

#include "JSONVisitor.h"

NCKPTranslator::NCKPTranslator(const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets)
	: m_builtin_widgets(m_builtin_widgets) {}

void NCKPTranslator::visit(JSONBool &boolean) {

}

void NCKPTranslator::visit(JSONString &str) {

    if(m_current_property == "\"id\"") {
		std::string var_name = remove_quotes(str.value);
		if(!m_panel_prefixes.empty()) {
			var_name = m_panel_prefixes.top().first + "_" + var_name;
		}
        m_ui_controls.insert({var_name, m_current_control_idx});
		if(m_current_control_idx == 0) {
			m_panel_prefixes.emplace(var_name, m_current_panel_object);
		}
    }
}

void NCKPTranslator::visit(JSONInt &num) {
    if(m_current_property == "\"index\"") {
        m_current_control_idx = num.value;
		if(m_current_control_idx == 0) {
			m_current_panel_object = m_current_object;
		}
    }
}

void NCKPTranslator::visit(JSONFloat &num) {

}

void NCKPTranslator::visit(JSONArray &array) {
    for(auto &elem: array.elements) {
        elem->accept(*this);
    }
}

void NCKPTranslator::visit(JSONObject &object) {
	m_current_object = &object;
    for(auto &pair : object.properties) {
        m_current_property = pair.first;
        pair.second->accept(*this);
    }
	if(!m_panel_prefixes.empty() and &object == m_panel_prefixes.top().second) {
		m_panel_prefixes.pop();
	}
}

std::vector<std::unique_ptr<NodeUIControl>> NCKPTranslator::collect_ui_variables() {
	std::vector<std::unique_ptr<NodeUIControl>> ui_variables;
	for(auto &ui_pair : m_ui_controls) {
		auto it = UI_CONTROL_INDEX.find(ui_pair.second);
		if(it == UI_CONTROL_INDEX.end()) {
			CompileError(ErrorType::ParseError, "Could not find ui widget index.", -1, "", std::to_string(ui_pair.second), "*.nckp").exit();
		}
		std::string ui_control = it->second;
		std::string ui_var = ui_pair.first;
		auto node_ast = m_builtin_widgets.find(ui_control)->second->clone();
		auto node_ui_control = std::unique_ptr<NodeUIControl>(static_cast<NodeUIControl*>(node_ast.release()));
		if(auto node_variable = cast_node<NodeVariable>(node_ui_control->control_var.get())) {
			node_variable->name = ui_var;
		} else if(auto node_array  = cast_node<NodeArray>(node_ui_control->control_var.get())) {
			node_array->name = ui_var;
		}
		auto num_params = node_ui_control->params->params.size();
		node_ui_control->params->params.clear();
		for(int i = 0; i<num_params; i++) {
			auto node_int = std::make_unique<NodeInt>(0, node_ui_control->tok);
			node_ui_control->params->params.push_back(std::move(node_int));
		}
		node_ui_control->update_parents(nullptr);
		ui_variables.push_back(std::move(node_ui_control));
	}
	return std::move(ui_variables);
}


