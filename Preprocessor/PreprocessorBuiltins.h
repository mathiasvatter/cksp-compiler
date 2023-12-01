//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorBuiltins : public Preprocessor {
public:
    explicit PreprocessorBuiltins(const std::string& builtin_vars, const std::string& builtin_functions);
    Result<SuccessTag> parse_builtin_variables(const std::string &file);
    Result<SuccessTag> parse_builtin_functions(const std::string &file);

    void process_builtins();

    std::unique_ptr<NodeVariable> parse_builtin_variable();
    std::unique_ptr<NodeArray> parse_builtin_array();
    Result<std::unique_ptr<NodeFunctionHeader>> parse_builtin_function();
    Result<std::pair<std::vector<ASTType>, std::vector<VarType>>> parse_builtin_args_list();

    [[nodiscard]] const std::vector<std::unique_ptr<NodeVariable>> &get_builtin_variables() const;
    [[nodiscard]] const std::vector<std::unique_ptr<NodeArray>> &get_builtin_arrays() const;
    [[nodiscard]] const std::vector<std::unique_ptr<NodeFunctionHeader>> &get_builtin_functions() const;

private:
    std::vector<std::unique_ptr<NodeVariable>> m_builtin_variables;
    std::vector<std::unique_ptr<NodeArray>> m_builtin_arrays;
    std::vector<std::unique_ptr<NodeFunctionHeader>> m_builtin_functions;

    std::string m_builtin_variables_file;
    std::string m_builtin_functions_file;

    static ASTType get_identifier_type(char identifier);
    ASTType get_type_annotation();
    static VarType get_var_type_annotation(const std::string& keyword);
};

