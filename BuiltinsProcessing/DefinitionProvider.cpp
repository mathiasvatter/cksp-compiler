//
// Created by Mathias Vatter on 05.04.24.
//

#include "DefinitionProvider.h"

#include <utility>

DefinitionProvider::DefinitionProvider(
		std::unordered_map<std::string, std::shared_ptr<NodeVariable>> m_builtin_variables,
		std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_builtin_functions,
		std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> m_boolean_functions,
		std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> m_property_functions,
		std::unordered_map<std::string, std::shared_ptr<NodeArray>> m_builtin_arrays,
		std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> m_builtin_widgets,
		std::vector<std::shared_ptr<NodeDataStructure>> m_external_variables)
        : external_variables(std::move(m_external_variables)),
		builtin_variables(std::move(m_builtin_variables)),
		builtin_arrays(std::move(m_builtin_arrays)), // property functions
		builtin_widgets(std::move(m_builtin_widgets)),
		builtin_functions(std::move(m_builtin_functions)),
		boolean_functions(std::move(m_boolean_functions)),
		property_functions(std::move(m_property_functions)) {
	// add default scope to work as global scope
	this->add_scope();
    add_external_variables_to_global_scope();
}

DefinitionProvider::DefinitionProvider() {
	// add default scope to work as global scope
	this->add_scope();
	add_external_variables_to_global_scope();
}

bool DefinitionProvider::add_scope() {
    m_declared_data_structures.emplace_back();
    return true;
}

bool DefinitionProvider::copy_last_scope() {
	if(m_declared_data_structures.size() < 2) {
		auto compile_error = CompileError(ErrorType::InternalError, "",-1, "","","");
		compile_error.m_message = "Tried to copy last scope, but there is no last scope to copy.";
		compile_error.exit();
		return false;
	}
	const auto& last_scope = m_declared_data_structures[m_declared_data_structures.size() - 2];
	auto& current_scope = m_declared_data_structures.back();
	// Optional: Reserve Platz im aktuellen Scope, um unnötige Allokationen zu vermeiden
	current_scope.reserve(current_scope.size() + last_scope.size());
	for (const auto& data_struct : last_scope) {
		current_scope.emplace(data_struct);
	}
	return true;
}

std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>, StringHash, StringEqual>
    DefinitionProvider::remove_scope() {
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
    // add global scope
    add_scope();
    add_external_variables_to_global_scope();
    return true;
}

std::shared_ptr<NodeDataStructure> DefinitionProvider::remove_from_current_scope(const std::string& name) {
    const auto it = m_declared_data_structures.back().find(name);
    if(it != m_declared_data_structures.back().end()) {
        auto var = it->second;
        m_declared_data_structures.back().erase(it);
        return var;
    }
    return nullptr;
}


std::shared_ptr<NodeDataStructure> DefinitionProvider::get_declaration(NodeReference& var) {
	// if reference is compiler, return dummy declaration pointer
	if(const auto &dummy_decl = get_compiler_declaration(var)) {
		var.kind = NodeReference::Kind::Compiler;
		return dummy_decl;
	}
	if(const auto &pgs_decl = get_pgs_declaration(var)) {
		var.kind = NodeReference::Kind::User;
		return pgs_decl;
	}
	// if(const auto &throwaway = get_throwaway_declaration(var)) {
	// 	var.kind = NodeReference::Kind::Throwaway;
	// 	return throwaway;
	// }

	// get builtin declaration if it exists
	std::shared_ptr<NodeDataStructure> node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var.name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var.name);

	if (node_builtin_declaration) {
		var.kind = NodeReference::Kind::Builtin;
		return node_builtin_declaration;
	}

	// try not sanitized name first
	auto node_declaration = get_declared_data_structure(var.name);
	if(!node_declaration) {
		// sanitize name if array
		const std::string sanitized = var.sanitize_name();
		node_declaration = get_declared_data_structure(sanitized);
	}
	if (node_declaration) {
		var.kind = NodeReference::Kind::User;
		return node_declaration;
	}
	return nullptr;
}

std::shared_ptr<NodeDataStructure> DefinitionProvider::set_declaration(const std::shared_ptr<NodeDataStructure>& var, bool global_scope) {
	// handle_throwaway_var(*var);
	m_gensym.ingest(var->name);
	// get builtin declaration if it exists
	std::shared_ptr<NodeDataStructure> node_builtin_declaration = nullptr;
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_array(var->name);
	if (!node_builtin_declaration) node_builtin_declaration = get_builtin_variable(var->name);

	// is declaration and is builtin -> compile error
	if (node_builtin_declaration) {
		auto compile_error = CompileError(ErrorType::VariableError, "", "", var->tok);
		compile_error.m_message = "Variable shadows builtin variable. Try renaming the variable.";
		compile_error.exit();
	}

	// input var is declaration
	if (auto data_struct = get_scoped_data_structure(var->name, global_scope)) {
		auto compile_error = CompileError(ErrorType::VariableError, "", "", var->tok);
		compile_error.m_message = "Data Structure has already been declared in this scope. Variables with different types but same names are not allowed. ";
		if (data_struct->is_engine) {
			compile_error.m_message = "A variable of this name is needed internally by cksp. ";
		} else {
			compile_error.m_message += var->name + " is a redeclaration of " + data_struct->tok.val + " in line " + std::to_string(data_struct->tok.line) +". ";
		}
		if(data_struct->is_function_param()) {
			compile_error.m_message += "The original declaration is a function parameter. Function parameters cannot be shadowed.";
		} else {
			compile_error.m_message += "Try renaming the variable to avoid shadowing.";
		}
        if(global_scope and !data_struct->is_engine) compile_error.m_message += "\nVariables declared in the <init> callback are always considered global, no local scopes are created.";
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


std::shared_ptr<NodeVariable> DefinitionProvider::get_builtin_variable(const std::string& var) {
	if(const auto it = builtin_variables.find(var); it != builtin_variables.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<NodeArray> DefinitionProvider::get_builtin_array(const std::string& arr) {
	if(const auto it = builtin_arrays.find(arr); it != builtin_arrays.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<NodeUIControl> DefinitionProvider::get_builtin_widget(const std::string &ui_control) {
	if(const auto it = builtin_widgets.find(ui_control); it != builtin_widgets.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> DefinitionProvider::get_builtin_function(NodeFunctionHeaderRef *function) {
	const auto it = builtin_functions.find({function->name, (int)function->args->size()});
	if(it != builtin_functions.end()) {
		return it->second;
	}
	return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> DefinitionProvider::get_boolean_function(const std::string &name, const int arg_count) {
	const auto it = boolean_functions.find({name, arg_count});
	if(it != boolean_functions.end()) {
		return it->second;
	}
	return nullptr;
}

std::shared_ptr<NodeFunctionDefinition> DefinitionProvider::get_property_function(NodeFunctionHeaderRef *function) {
	if(auto it = property_functions.find(function->name); it != property_functions.end()) {
		return it->second;
	}
	return nullptr;
}

std::shared_ptr<NodeDataStructure> DefinitionProvider::get_declared_data_structure(const std::string& data) {
	// then search in all other scopes
    for (auto rit = m_declared_data_structures.rbegin(); rit != m_declared_data_structures.rend(); ++rit) {
	    if (auto it = rit->find(data); it != rit->end()) {
            return it->second;
        }
    }
    return nullptr;
}

std::shared_ptr<NodeDataStructure> DefinitionProvider::get_scoped_data_structure(const std::string& data, const bool global_scope) const {
    const auto& map = global_scope ? m_declared_data_structures.at(0) : m_declared_data_structures.back();
    if (const auto it = map.find(data); it != map.end()) {
		return it->second;
	}
	return nullptr;
}

void DefinitionProvider::set_external_variables(std::vector<std::shared_ptr<NodeDataStructure>> external_variables) {
	DefinitionProvider::external_variables = std::move(external_variables);
}

void DefinitionProvider::set_builtin_variables(std::unordered_map<std::string, std::shared_ptr<NodeVariable>> builtin_variables) {
	DefinitionProvider::builtin_variables = std::move(builtin_variables);
}

void DefinitionProvider::set_builtin_arrays(std::unordered_map<std::string, std::shared_ptr<NodeArray>> builtin_arrays) {
	DefinitionProvider::builtin_arrays = std::move(builtin_arrays);
}

void DefinitionProvider::set_builtin_widgets(std::unordered_map<std::string, std::shared_ptr<NodeUIControl>> builtin_widgets) {
	DefinitionProvider::builtin_widgets = std::move(builtin_widgets);
}

void DefinitionProvider::set_builtin_functions(std::unordered_map<StringIntKey, std::shared_ptr<NodeFunctionDefinition>, StringIntKeyHash> builtin_functions) {
	DefinitionProvider::builtin_functions = std::move(builtin_functions);
}

void DefinitionProvider::set_property_functions(std::unordered_map<std::string, std::shared_ptr<NodeFunctionDefinition>> property_functions) {
	DefinitionProvider::property_functions = std::move(property_functions);
}



