//
// Created by Mathias Vatter on 05.04.24.
//

#pragma once

#include "ASTVisitor.h"

enum Definition {
    Reference,
    Declaration,
    Builtin
};


class DefinitionProvider {
public:
    DefinitionProvider(
            const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &m_builtin_variables,
            const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions,
			const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>>& m_property_functions,
            const std::unordered_map<std::string, std::unique_ptr<NodeArray>> &m_builtin_arrays,
            const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets,
            const std::vector<std::unique_ptr<DataStructure>> &m_external_variables);


    /// external variables from eg nckp file
    const std::vector<std::unique_ptr<DataStructure>> &external_variables;
    /// builtin engine variables
    const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &builtin_variables;
    NodeVariable* get_builtin_variable(const std::string& var);
    /// builtin engine arrays
    const std::unordered_map<std::string, std::unique_ptr<NodeArray>> &builtin_arrays;
    NodeArray* get_builtin_array(const std::string& arr);
    /// builtin engine widgets
    const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &builtin_widgets;
    NodeUIControl* get_builtin_widget(const std::string &ui_control);
    /// builtin engine functions
    const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &builtin_functions;
    NodeFunctionHeader* get_builtin_function(const std::string &function, int params);
    NodeFunctionHeader* get_builtin_function(NodeFunctionHeader* function);
	/// predefined property functions like set_label_properties etc
	const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> &property_functions;
	NodeFunctionHeader* get_property_function(NodeFunctionHeader* function);

private:
	bool add_scope();
	bool remove_scope();

	void match_data_structure(DataStructure* reference, DataStructure* declaration);
//    std::unique_ptr<DataStructure> build_data_structure(std::unique_ptr<NodeVariable> var, DataStructure* declaration);
//    std::unique_ptr<DataStructure> build_data_structure(std::unique_ptr<NodeArray> var, DataStructure* declaration);

	/// returns the definition of a data structure, if it exists
	DataStructure* get_declaration(DataStructure* var);

	/// declared variables
    std::vector<std::unordered_map<std::string, NodeVariable*, StringHash, StringEqual>> m_declared_variables;
    NodeVariable* get_declared_variable(const std::string& var);
    /// declared arrays
    std::vector<std::unordered_map<std::string, NodeArray*, StringHash, StringEqual>> m_declared_arrays;
    // when is variable = raw array? if variable has _ in front and is array and was declared without _
    NodeArray* get_raw_declared_array(const std::string& var);
    NodeArray* get_declared_array(const std::string& arr);

    /// declared everything
    std::vector<std::unordered_map<std::string, DataStructure*, StringHash, StringEqual>> m_declared_data_structures;
    DataStructure* get_declared_data_structure(const std::string& data);


    /// declared ui_controls
    std::vector<std::unordered_map<std::string, NodeUIControl*, StringHash, StringEqual>> m_declared_controls;
    NodeUIControl* get_declared_control(NodeUIControl* arr);
};


