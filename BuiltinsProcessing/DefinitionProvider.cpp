//
// Created by Mathias Vatter on 05.04.24.
//

#include "DefinitionProvider.h"

#include <utility>

DefinitionProvider::DefinitionProvider(
		std::unordered_map<std::string, std::unique_ptr<NodeVariable>> m_builtin_variables,
		std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_builtin_functions,
		std::unordered_map<std::string, std::unique_ptr<NodeFunctionDefinition>> m_property_functions,
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
    for(const auto& var : external_variables) {
        m_declared_data_structures.back().insert({var->name, var.get()});
    }
}

DefinitionProvider::DefinitionProvider() {
	// add default scope to work as global scope
	this->add_scope();
	for(const auto& var : external_variables) {
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
	auto passive_scope = std::move(m_declared_data_structures.back());
	m_declared_data_structures.pop_back();
	return passive_scope;
}

bool DefinitionProvider::refresh_scopes() {
    m_declared_data_structures.clear();
	m_references_per_data_structure.clear();
    // add global scope
    add_scope();
    for(const auto& var : external_variables) {
        m_declared_data_structures.back().insert({var->name, var.get()});
    }
    return true;
}

//bool DefinitionProvider::clear_all_reference_sets() {
//	for(auto& var : m_all_data_structures) {
//		var->references.clear();
//	}
//	m_all_data_structures.clear();
//	m_all_references.clear();
//	return true;
//}


NodeDataStructure* DefinitionProvider::remove_from_current_scope(const std::string& name) {
    const auto it = m_declared_data_structures.back().find(name);
    if(it != m_declared_data_structures.back().end()) {
        auto var = it->second;
        m_declared_data_structures.back().erase(it);
        return var;
    }
    return nullptr;
}


NodeDataStructure* DefinitionProvider::get_declaration(NodeReference* var) {
	// if reference is compiler, return dummy declaration pointer
	if(const auto &dummy_decl = get_compiler_declaration(var)) {
		var->kind = NodeReference::Kind::Compiler;
		return dummy_decl;
	}
	if(const auto &pgs_decl = get_pgs_declaration(var)) {
		var->kind = NodeReference::Kind::User;
		return pgs_decl;
	}
	if(const auto &throwaway = get_throwaway_declaration(var)) {
		var->kind = NodeReference::Kind::Throwaway;
		return throwaway;
	}

	// get builtin declaration if it exists
	NodeDataStructure *node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var->name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var->name);

	if (node_builtin_declaration) {
		var->kind = NodeReference::Kind::Builtin;
		return node_builtin_declaration;
	}

	// try not sanitized name first
	auto node_declaration = get_declared_data_structure(var->name);
	if(!node_declaration) {
		// sanitize name if array
		const std::string sanitized = var->sanitize_name();
		node_declaration = get_declared_data_structure(sanitized);
	}
	if (node_declaration) {
		m_references_per_data_structure[node_declaration].insert(var);
		var->kind = NodeReference::Kind::User;
		return node_declaration;
	}
	return nullptr;
}

const NodeDataStructure* DefinitionProvider::set_declaration(NodeDataStructure* var, bool global_scope) {
	handle_throwaway_var(*var);
	m_gensym.ingest(var->name);
	// get builtin declaration if it exists
	NodeDataStructure *node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var->name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var->name);

	// is declaration and is builtin -> compile error
	if (node_builtin_declaration) {
		auto compile_error = CompileError(ErrorType::Variable, "",var->tok.line, "", var->name, var->tok.file);
		compile_error.m_message = "Variable shadows builtin variable. Try renaming the variable.";
		compile_error.exit();
	}

	// input var is declaration
	if (auto data_struct = get_scoped_data_structure(var->name, global_scope)) {
		auto compile_error = CompileError(ErrorType::Variable, "",var->tok.line, "", var->name, var->tok.file);
		compile_error.m_message = "Data Structure has already been declared in this scope. Variables with different types but same names are not allowed. " +
			var->name + " is a redeclaration of " + data_struct->get_string() + " in line " + std::to_string(data_struct->tok.line) + ".";
        if(global_scope) compile_error.m_message += "\nVariables declared in the <init> callback are always considered global, no local scopes are created.";
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


// ******************* getter and setter *******************


NodeVariable* DefinitionProvider::get_builtin_variable(const std::string& var) {
    const auto it = builtin_variables.find(var);
    if(it != builtin_variables.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeArray* DefinitionProvider::get_builtin_array(const std::string& arr) {
    const auto it = builtin_arrays.find(arr);
    if(it != builtin_arrays.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeUIControl* DefinitionProvider::get_builtin_widget(const std::string &ui_control) {
    const auto it = builtin_widgets.find(ui_control);
    if(it != builtin_widgets.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionDefinition* DefinitionProvider::get_builtin_function(const std::string &function, int params) {
    const auto it = builtin_functions.find({function, params});
    if(it != builtin_functions.end()) {
        return it->second.get();
    }
    return nullptr;
}

NodeFunctionDefinition* DefinitionProvider::get_builtin_function(NodeFunctionHeaderRef *function) {
	const auto it = builtin_functions.find({function->name, (int)function->args->size()});
	if(it != builtin_functions.end()) {
		return it->second.get();
	}
	return nullptr;
}

NodeFunctionDefinition* DefinitionProvider::get_property_function(NodeFunctionHeaderRef *function) {
	auto it = property_functions.find(function->name);
	if(it != property_functions.end()) {
		return it->second.get();
	}
	return nullptr;
}

NodeDataStructure *DefinitionProvider::get_declared_data_structure(const std::string& data) {
	// then search in all other scopes
    for (auto rit = m_declared_data_structures.rbegin(); rit != m_declared_data_structures.rend(); ++rit) {
		auto it = rit->find(data);
        if (it != rit->end()) {
            return it->second;
        }
    }
    return nullptr;
}

NodeDataStructure *DefinitionProvider::get_scoped_data_structure(const std::string& data, bool global_scope) {
    const auto& map = global_scope ? m_declared_data_structures.at(0) : m_declared_data_structures.back();
	auto it = map.find(data);
	if (it != map.end()) {
		return it->second;
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

void DefinitionProvider::set_builtin_functions(std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionDefinition>, StringIntKeyHash> builtin_functions) {
	DefinitionProvider::builtin_functions = std::move(builtin_functions);
}

void DefinitionProvider::set_property_functions(std::unordered_map<std::string, std::unique_ptr<NodeFunctionDefinition>> property_functions) {
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

void DefinitionProvider::add_builtin_function(std::unique_ptr<NodeFunctionDefinition> builtin_function) {
    builtin_functions[{builtin_function->header->name, (int)builtin_function->header->params.size()}] = std::move(builtin_function);
}

void DefinitionProvider::add_property_function(std::unique_ptr<NodeFunctionDefinition> property_function) {
    property_functions.insert({property_function->header->name, std::move(property_function)});
}
