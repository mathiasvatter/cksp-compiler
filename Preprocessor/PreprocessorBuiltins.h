//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorBuiltins : public Preprocessor {
public:
	PreprocessorBuiltins(const std::string& builtin_vars, const std::string& builtin_functions, const std::string& builtin_widgets);
    Result<SuccessTag> parse_builtin_variables(const std::string &file);
    Result<SuccessTag> parse_builtin_functions(const std::string &file);
	Result<SuccessTag> parse_builtin_widgets(const std::string &file);

    void process_builtins();

    std::unique_ptr<NodeVariable> parse_builtin_variable();
    std::unique_ptr<NodeArray> parse_builtin_array();
    Result<std::unique_ptr<NodeFunctionHeader>> parse_builtin_function();
	Result<std::unique_ptr<NodeUIControl>> parse_builtin_ui_control();
    Result<std::pair<std::vector<ASTType>, std::vector<VarType>>> parse_builtin_args_list(std::unique_ptr<NodeParamList>& func_args);

    [[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<NodeVariable>> &get_builtin_variables() const;
    [[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<NodeArray>> &get_builtin_arrays() const;
    [[nodiscard]] const std::vector<std::unique_ptr<NodeFunctionHeader>> &get_builtin_functions() const;
    [[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> &get_property_functions() const;
	[[nodiscard]] const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &get_builtin_widgets() const;


private:
    std::unordered_map<std::string, std::unique_ptr<NodeVariable>> m_builtin_variables;
//    std::vector<std::unique_ptr<NodeVariable>> m_builtin_variables;
    std::unordered_map<std::string, std::unique_ptr<NodeArray>> m_builtin_arrays;
//    std::vector<std::unique_ptr<NodeArray>> m_builtin_arrays;
    std::vector<std::unique_ptr<NodeFunctionHeader>> m_builtin_functions;
//    std::vector<std::unique_ptr<NodeFunctionHeader>> m_builtin_functions;
    std::unordered_map<std::string, std::unique_ptr<NodeFunctionHeader>> m_property_functions;
//    std::vector<std::unique_ptr<NodeFunctionHeader>> m_property_functions;
    std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> m_builtin_widgets;
//	std::vector<std::unique_ptr<NodeUIControl>> m_builtin_widgets;


    std::string m_builtin_variables_file;
    std::string m_builtin_functions_file;
	std::string m_builtin_widgets_file;


    static ASTType get_identifier_type(char identifier);
    static ASTType get_type_annotation(const Token& tok);
    static VarType get_var_type_annotation(const std::string& keyword);
    static bool is_property_function(const std::string& fun_name);
};

