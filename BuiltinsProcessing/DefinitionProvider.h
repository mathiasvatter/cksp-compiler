//
// Created by Mathias Vatter on 05.04.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

enum class Definition {
    Reference,
    Declaration,
    Builtin
};


class DefinitionProvider {
public:
	/// Collects all definitions of builtin variables, arrays, functions, widgets and external variables and
	/// provides them to the ASTVisitors including dedicated methods to search for the definitions
    DefinitionProvider(
			std::unordered_map<std::string, std::unique_ptr<NodeVariable>> m_builtin_variables,
			std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> m_builtin_functions,
			std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> m_property_functions,
			std::unordered_map<std::string, std::unique_ptr<NodeArray>> m_builtin_arrays,
			std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> m_builtin_widgets,
			std::vector<std::unique_ptr<NodeDataStructure>> m_external_variables);
	explicit DefinitionProvider() = default;

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
	bool remove_scope();

	void match_data_structure(NodeDataStructure* reference, NodeDataStructure* declaration);
//    std::unique_ptr<NodeDataStructure> build_data_structure(std::unique_ptr<NodeVariable> var, NodeDataStructure* declaration);
//    std::unique_ptr<NodeDataStructure> build_data_structure(std::unique_ptr<NodeArray> var, NodeDataStructure* declaration);

	/// returns the definition of a data structure, if it exists. If datastructure itself is
	/// definition -> returns itself
	NodeDataStructure* get_declaration(NodeDataStructure* var);

	/// declared variables
    std::vector<std::unordered_map<std::string, NodeVariable*, StringHash, StringEqual>> m_declared_variables;
    NodeVariable* get_declared_variable(const std::string& var);
    /// declared arrays
    std::vector<std::unordered_map<std::string, NodeArray*, StringHash, StringEqual>> m_declared_arrays;
    // when is variable = raw array? if variable has _ in front and is array and was declared without _
    NodeArray* get_raw_declared_array(const std::string& var);
    NodeArray* get_declared_array(const std::string& arr);

    /// declared everything
    std::vector<std::unordered_map<std::string, std::unique_ptr<NodeDataStructure>, StringHash, StringEqual>> m_declared_data_structures;
    NodeDataStructure* get_declared_data_structure(const std::string& data);


    /// declared ui_controls
    std::vector<std::unordered_map<std::string, NodeUIControl*, StringHash, StringEqual>> m_declared_controls;
    NodeUIControl* get_declared_control(NodeUIControl* arr);
};


