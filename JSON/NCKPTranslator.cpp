//
// Created by Mathias Vatter on 02.01.24.
//

#include "NCKPTranslator.h"

namespace {
	template<typename T>
	T* get_property_as(JSONObject& object, const std::string& key) {
		auto it = object.properties.find(key);
		if(it == object.properties.end()) {
			return nullptr;
		}
		return dynamic_cast<T*>(it->second.get());
	}

	JSONString* find_control_id(JSONObject& object) {
		if(auto id = get_property_as<JSONString>(object, "id")) {
			return id;
		}
		if(auto common = get_property_as<JSONObject>(object, "common")) {
			if(auto id = get_property_as<JSONString>(*common, "id")) {
				return id;
			}
		}
		if(auto value = get_property_as<JSONObject>(object, "value")) {
			if(auto id = get_property_as<JSONString>(*value, "id")) {
				return id;
			}
			if(auto common = get_property_as<JSONObject>(*value, "common")) {
				return get_property_as<JSONString>(*common, "id");
			}
		}
		return nullptr;
	}
}

NCKPTranslator::NCKPTranslator(DefinitionProvider* definition_provider)
	: m_def_provider(definition_provider) {}

void NCKPTranslator::visit(JSONBool &boolean) {

}

void NCKPTranslator::visit(JSONString &str) {
}

void NCKPTranslator::visit(JSONInt &num) {
}

void NCKPTranslator::visit(JSONFloat &num) {

}

void NCKPTranslator::visit(JSONArray &array) {
    for(auto &elem: array.elements) {
        elem->accept(*this);
    }
}

void NCKPTranslator::visit(JSONObject &object) {
	bool pushed_panel = false;
	if(auto index = get_property_as<JSONInt>(object, "index")) {
		if(auto id = find_control_id(object)) {
			std::string var_name = StringUtils::remove_quotes(id->value);
			if(!m_panel_prefixes.empty()) {
				var_name = m_panel_prefixes.top().first + "_" + var_name;
			}
			m_ui_controls.insert({var_name, index->value});
			if(index->value == 0) {
				m_panel_prefixes.emplace(var_name, &object);
				pushed_panel = true;
			}
		}
	}

	for(auto &pair : object.properties) {
		pair.second->accept(*this);
	}
	if(pushed_panel) {
		m_panel_prefixes.pop();
	}
}

std::vector<std::shared_ptr<NodeDataStructure>> NCKPTranslator::collect_ui_variables() const {
	std::vector<std::shared_ptr<NodeDataStructure>> ui_variables;
	for(auto &ui_pair : m_ui_controls) {
		auto it = UI_CONTROL_INDEX.find(ui_pair.second);
		if(it == UI_CONTROL_INDEX.end()) {
			Diagnostic(ErrorType::ParseError, "Could not find ui widget index.", -1, "", std::to_string(ui_pair.second), "*.nckp").exit();
		}
		std::string ui_control = it->second;
		std::string ui_var = ui_pair.first;
		auto node_ast = m_def_provider->builtin_widgets.find(ui_control)->second->clone();
		auto node_ui_control = unique_ptr_cast<NodeUIControl>(std::move(node_ast));
		if(auto node_variable = cast_node<NodeVariable>(node_ui_control->control_var.get())) {
			node_variable->name = ui_var;
            node_variable->data_type = DataType::UIControl;
		} else if(auto node_array  = cast_node<NodeArray>(node_ui_control->control_var.get())) {
			node_array->name = ui_var;
            node_array->data_type = DataType::UIControl;
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
