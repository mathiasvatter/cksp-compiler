//
// Created by Mathias Vatter on 05.04.24.
//

#include "DefinitionProvider.h"

#include <utility>

DefinitionProvider::DefinitionProvider(
		std::unordered_map<std::string, std::unique_ptr<NodeVariable>> m_builtin_variables,
		std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> m_builtin_functions,
		std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> m_property_functions,
		std::unordered_map<std::string, std::unique_ptr<NodeArray>> m_builtin_arrays,
		std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> m_builtin_widgets,
		std::vector<std::unique_ptr<NodeDataStructure>> m_external_variables)
        : builtin_variables(std::move(m_builtin_variables)),
		  builtin_functions(std::move(m_builtin_functions)),
		  property_functions(std::move(m_property_functions)), // property functions
		  builtin_arrays(std::move(m_builtin_arrays)),
		  builtin_widgets(std::move(m_builtin_widgets)),
		  external_variables(std::move(m_external_variables)) {
	// add default scope to work as global scope
	this->add_scope();
    for(auto& var : external_variables) {
        m_declared_data_structures.back().insert({var->name, var.get()});
    }
}

DefinitionProvider::DefinitionProvider() {
	// add default scope to work as global scope
	this->add_scope();
	for(auto& var : external_variables) {
		m_declared_data_structures.back().insert({var->name, var.get()});
	}
}

bool DefinitionProvider::add_scope() {
    m_declared_data_structures.emplace_back();
    return true;
}

std::unordered_map<std::string, NodeDataStructure*, StringHash, StringEqual> DefinitionProvider::remove_scope() {
    auto compile_error = CompileError(ErrorType::InternalError, "",-1, "","","");
    if(m_declared_data_structures.empty()) {
        compile_error.m_message = "Tried to remove global scope.";
        compile_error.exit();
    }
    auto passive_scope = m_declared_data_structures.back();
    m_declared_data_structures.pop_back();
    return passive_scope;
}

bool DefinitionProvider::refresh_scopes() {
    m_declared_variables.clear();
    m_declared_arrays.clear();
    m_declared_controls.clear();
    m_declared_data_structures.clear();
    // add global scope
    add_scope();
    for(auto& var : external_variables) {
        m_declared_data_structures.back().insert({var->name, var.get()});
    }
    return true;
}

NodeDataStructure* DefinitionProvider::remove_from_current_scope(const std::string& name) {
    auto it = m_declared_data_structures.back().find(name);
    if(it != m_declared_data_structures.back().end()) {
        auto var = it->second;
        m_declared_data_structures.back().erase(it);
        return var;
    }
    return nullptr;
}


NodeDataStructure* DefinitionProvider::get_declaration(NodeReference* var) {
	// get builtin declaration if it exists
	NodeDataStructure *node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var->name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var->name);

	auto compile_error = CompileError(ErrorType::Variable, "",var->tok.line, "", var->name, var->tok.file);

	if (node_builtin_declaration) {
		return node_builtin_declaration;
	} else if (auto node_declaration = get_declared_data_structure(var->name)) {
		return node_declaration;
	}
	return nullptr;
}

NodeDataStructure* DefinitionProvider::set_declaration(NodeDataStructure* var, bool global_scope) {
	// get builtin declaration if it exists
	NodeDataStructure *node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var->name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var->name);

	auto compile_error = CompileError(ErrorType::Variable, "",var->tok.line, "", var->name, var->tok.file);
	// is declaration and is builtin -> compile error
	if (node_builtin_declaration) {
		compile_error.m_message = "Variable shadows builtin variable. Try renaming the variable.";
		compile_error.exit();
	}

	// input var is declaration
	if (get_scoped_data_structure(var->name)) {
		compile_error.m_message = "Data Structure has already been declared in this scope.";
		compile_error.exit();
	} else {
		if(global_scope) {
			m_declared_data_structures.at(0).insert({var->name, var});
		} else {
			m_declared_data_structures.back().insert({var->name, var});
		}
	}
	return nullptr;
}


NodeVariable* DefinitionProvider::get_builtin_variable(const std::string& var) {
    auto it = builtin_variables.find(var);
    if(it != builtin_variables.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeArray* DefinitionProvider::get_builtin_array(const std::string& arr) {
    auto it = builtin_arrays.find(arr);
    if(it != builtin_arrays.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeUIControl* DefinitionProvider::get_builtin_widget(const std::string &ui_control) {
    auto it = builtin_widgets.find(ui_control);
    if(it != builtin_widgets.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionHeader* DefinitionProvider::get_builtin_function(const std::string &function, int params) {
    auto it = builtin_functions.find({function, params});
    if(it != builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionHeader* DefinitionProvider::get_builtin_function(NodeFunctionHeader *function) {
	auto it = builtin_functions.find({function->name, (int)function->args->params.size()});
	if(it != builtin_functions.end()) {
		return it->second.get();
	}
	return nullptr;
}

NodeFunctionHeader* DefinitionProvider::get_property_function(NodeFunctionHeader *function) {
	auto it = property_functions.find(function->name);
	if(it != property_functions.end()) {
		return it->second.get();
	}
	return nullptr;
}

NodeVariable *DefinitionProvider::get_declared_variable(const std::string& var) {
    for (auto rit = m_declared_variables.rbegin(); rit != m_declared_variables.rend(); ++rit) {
        auto it = rit->find(var);
        if (it != rit->end()) {
            return it->second;
        }
        return nullptr;
    }
    return nullptr;
}

NodeArray *DefinitionProvider::get_declared_array(const std::string& arr) {
    for (auto rit = m_declared_arrays.rbegin(); rit != m_declared_arrays.rend(); ++rit) {
        auto it = rit->find(arr);
        if (it != rit->end()) {
            return it->second;
        }
        return nullptr;
    }
    return nullptr;
}

NodeDataStructure *DefinitionProvider::get_declared_data_structure(const std::string& data) {
//    std::string var_without_identifier = data;
//    if (data[0] == '_' && data[1] != '_') {
//        var_without_identifier = var_without_identifier.erase(0,1);
//    } else if (data.ends_with(".raw")) {
//        var_without_identifier = var_without_identifier.replace(var_without_identifier.size()-4, 4, "");
//    }
    for (auto rit = m_declared_data_structures.rbegin(); rit != m_declared_data_structures.rend(); ++rit) {
        auto it = rit->find(data);
        if (it != rit->end()) {
            return it->second;
        }
    }
    return nullptr;
}

NodeDataStructure *DefinitionProvider::get_scoped_data_structure(const std::string& data) {
//	std::string var_without_identifier = data;
//	if (data[0] == '_' && data[1] != '_') {
//		var_without_identifier = var_without_identifier.erase(0,1);
//	} else if (data.ends_with(".raw")) {
//		var_without_identifier = var_without_identifier.replace(var_without_identifier.size()-4, 4, "");
//	}
	auto it = m_declared_data_structures.back().find(data);
	if (it != m_declared_data_structures.back().end()) {
		return it->second;
	}
	return nullptr;
}

NodeArray *DefinitionProvider::get_raw_declared_array(const std::string& var) {
    std::string var_without_identifier = var;
    if (var[0] == '_' && var[1] != '_') {
        var_without_identifier = var_without_identifier.erase(0,1);
    } else if (var.ends_with(".raw")) {
        var_without_identifier = var_without_identifier.replace(var_without_identifier.size()-4, 4, "");
    }
    return get_declared_array(var_without_identifier);
}

NodeUIControl *DefinitionProvider::get_declared_control(NodeUIControl *ctr) {
    for (auto rit = m_declared_controls.rbegin(); rit != m_declared_controls.rend(); ++rit) {
        auto it = rit->find(ctr->get_string());
        if (it != rit->end()) {
            return it->second;
        }
        return nullptr;
    }
    return nullptr;
}

void DefinitionProvider::set_external_variables(std::vector<std::unique_ptr<NodeDataStructure>> external_variables) {
	DefinitionProvider::external_variables = std::move(external_variables);
}

void DefinitionProvider::set_builtin_variables(std::unordered_map<std::string, std::unique_ptr<NodeVariable>> builtin_variables) {
	DefinitionProvider::builtin_variables = std::move(builtin_variables);
}

void DefinitionProvider::set_builtin_arrays(std::unordered_map<std::string, std::unique_ptr<NodeArray>> builtin_arrays) {
	DefinitionProvider::builtin_arrays = std::move(builtin_arrays);
}

void DefinitionProvider::set_builtin_widgets(std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> builtin_widgets) {
	DefinitionProvider::builtin_widgets = std::move(builtin_widgets);
}

void DefinitionProvider::set_builtin_functions(std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> builtin_functions) {
	DefinitionProvider::builtin_functions = std::move(builtin_functions);
}

void DefinitionProvider::set_property_functions(std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> property_functions) {
	DefinitionProvider::property_functions = std::move(property_functions);
}

void DefinitionProvider::add_external_variable(std::unique_ptr<NodeDataStructure> external_variable) {
    external_variables.push_back(std::move(external_variable));
}

void DefinitionProvider::add_builtin_variable(std::unique_ptr<NodeVariable> builtin_variable) {
    builtin_variables.insert({builtin_variable->name, std::move(builtin_variable)});
}

void DefinitionProvider::add_builtin_array(std::unique_ptr<NodeArray> builtin_array) {
    builtin_arrays.insert({builtin_array->name, std::move(builtin_array)});
}

void DefinitionProvider::add_builtin_widget(std::unique_ptr<NodeUIControl> builtin_widget) {
    builtin_widgets.insert({builtin_widget->name, std::move(builtin_widget)});
}

void DefinitionProvider::add_builtin_function(std::unique_ptr<NodeFunctionHeader> builtin_function) {
    builtin_functions[{builtin_function->name, (int)builtin_function->args->params.size()}] = std::move(builtin_function);
}

void DefinitionProvider::add_property_function(std::unique_ptr<NodeFunctionHeader> property_function) {
    property_functions.insert({property_function->name, std::move(property_function)});
}
