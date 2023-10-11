//
// Created by Mathias Vatter on 11.10.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorMacros : public Preprocessor {
public:
    PreprocessorMacros(std::vector<Token> tokens, const std::string &currentFile)
    : Preprocessor(std::move(tokens),currentFile) {}
    Result<SuccessTag> process_macros();

private:
    std::vector<std::unique_ptr<NodeMacroDefinition>> m_macro_definitions;
    std::vector<std::unique_ptr<NodeMacroHeader>> m_macro_calls;
    std::vector<std::unique_ptr<NodeIterateMacro>> m_macro_iterations;
    std::vector<std::string> m_macro_evaluation_stack;
    std::vector<Token> m_macro_tokens;

    // Macros
    Result<SuccessTag> process_macro_definitions();
    Result<SuccessTag> process_macro_calls();
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_header();
    Result<std::unique_ptr<NodeMacroDefinition>> parse_macro_definition();
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_call();
    Result<std::unique_ptr<NodeIterateMacro>> parse_iterate_macro();
    bool is_defined_macro(const std::string &name);
//	Result<std::unique_ptr<NodeMacroDefinition>&> get_defined_macro(std::unique_ptr<NodeMacroHeader> &macro_call);

    bool is_macro_call();
    Result<SuccessTag> evaluate_macro_call(std::unique_ptr<NodeMacroHeader> macro_call);

};

class SimpleInterpreter {
public:
    explicit SimpleInterpreter() {}
    Result<int> evaluate_int_expression(std::unique_ptr<NodeAST>& root);
};