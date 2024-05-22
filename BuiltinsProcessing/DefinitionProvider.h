//
// Created by Mathias Vatter on 05.04.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * @class DefinitionProvider
 *
 * @brief Collects all definitions of builtin variables, arrays, functions, widgets and external variables as well as
 * declared data structures and provides them to the ASTVisitors including dedicated methods to search for the
 * definitions.
 *
 * This class is responsible for providing definitions for builtin ksp definitions as well as
 * keeping track of declared variables, arrays, data structures and controls by adding them to the respective maps/scopes
 * and returning their declaration when needed. It also provides methods to search for declared variables, arrays,
 * data structures and controls.
 */
class DefinitionProvider {
public:
    DefinitionProvider(
			std::unordered_map<std::string, std::unique_ptr<NodeVariable>> m_builtin_variables,
			std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> m_builtin_functions,
			std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> m_property_functions,
			std::unordered_map<std::string, std::unique_ptr<NodeArray>> m_builtin_arrays,
			std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> m_builtin_widgets,
			std::vector<std::unique_ptr<NodeDataStructure>> m_external_variables);
	explicit DefinitionProvider();

    /// external variables from eg nckp file
	std::vector<std::unique_ptr<NodeDataStructure>> external_variables{};
	void set_external_variables(std::vector<std::unique_ptr<NodeDataStructure>> external_variables);
    void add_external_variable(std::unique_ptr<NodeDataStructure> external_variable);
	/// builtin engine variables
	std::unordered_map<std::string, std::unique_ptr<NodeVariable>> builtin_variables{};
    NodeVariable* get_builtin_variable(const std::string& var);
	void set_builtin_variables(std::unordered_map<std::string, std::unique_ptr<NodeVariable>> builtin_variables);
    void add_builtin_variable(std::unique_ptr<NodeVariable> builtin_variable);
    /// builtin engine arrays
	std::unordered_map<std::string, std::unique_ptr<NodeArray>> builtin_arrays{};
    NodeArray* get_builtin_array(const std::string& arr);
	void set_builtin_arrays(std::unordered_map<std::string, std::unique_ptr<NodeArray>> builtin_arrays);
    void add_builtin_array(std::unique_ptr<NodeArray> builtin_array);
    /// builtin engine widgets
	std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> builtin_widgets{};
    NodeUIControl* get_builtin_widget(const std::string &ui_control);
	void set_builtin_widgets(std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> builtin_widgets);
    void add_builtin_widget(std::unique_ptr<NodeUIControl> builtin_widget);
    /// builtin engine functions
	std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> builtin_functions{};
    NodeFunctionHeader* get_builtin_function(const std::string &function, int params);
    NodeFunctionHeader* get_builtin_function(NodeFunctionHeader* function);
	void set_builtin_functions(std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> builtin_functions);
    void add_builtin_function(std::unique_ptr<NodeFunctionHeader> builtin_function);
	/// predefined property functions like set_label_properties etc
	std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> property_functions{};
	NodeFunctionHeader* get_property_function(NodeFunctionHeader* function);
	void set_property_functions(std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> property_functions);
    void add_property_function(std::unique_ptr<NodeFunctionHeader> property_function);

	bool add_scope();
	std::unordered_map<std::string, NodeDataStructure*, StringHash, StringEqual> remove_scope();
	/// removes all scopes and initializes again
	bool refresh_scopes();
    /// removes variable from current scope by their name value
    NodeDataStructure* remove_from_current_scope(const std::string& name);
	/// copies last scope in current scope
	inline bool copy_last_scope() {
		auto compile_error = CompileError(ErrorType::InternalError, "",-1, "","","");
		if(m_declared_data_structures.size() < 2) {
			compile_error.m_message = "Tried to copy last scope, but there is no last scope to copy.";
			compile_error.exit();
			return false;
		}
		for(auto &data_struct : m_declared_data_structures.at(m_declared_data_structures.size()-2)) {
			m_declared_data_structures.back().emplace(data_struct);
		}
		return true;
	}

	/// returns the definition of a data structure, if it exists. If datastructure itself is
	/// definition -> return nullptr. If datastructure is reference -> return declaration. If global_scope is true,
	/// adds declaration to global scope.
	/// only called by references -> only gets declaration does not add existing declarations to map
	NodeDataStructure* get_declaration(NodeReference* var);
	/// adds existing declaration to declaration map for look up. Always returns nullptr.
	NodeDataStructure* set_declaration(NodeDataStructure* var, bool global_scope);

	/// declared variables
    std::vector<std::unordered_map<std::string, NodeVariable*, StringHash, StringEqual>> m_declared_variables;
    NodeVariable* get_declared_variable(const std::string& var);
    /// declared arrays
    std::vector<std::unordered_map<std::string, NodeArray*, StringHash, StringEqual>> m_declared_arrays;
    // when is variable = raw array? if variable has _ in front and is array and was declared without _
    NodeArray* get_raw_declared_array(const std::string& var);
    NodeArray* get_declared_array(const std::string& arr);

    /// declared everything
    std::vector<std::unordered_map<std::string, NodeDataStructure*, StringHash, StringEqual>> m_declared_data_structures;
	/// returns data structure declaration searching all scopes
    NodeDataStructure* get_declared_data_structure(const std::string& data);
	/// only returns data structure declaration in current scope or global_scope
	NodeDataStructure* get_scoped_data_structure(const std::string& data);


    /// declared ui_controls
    std::vector<std::unordered_map<std::string, NodeUIControl*, StringHash, StringEqual>> m_declared_controls;
    NodeUIControl* get_declared_control(NodeUIControl* arr);
};


