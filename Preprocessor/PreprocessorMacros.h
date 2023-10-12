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
    std::vector<std::unique_ptr<NodeIterateMacro>> m_macro_iterations;
    std::vector<std::string> m_macro_evaluation_stack;
    std::vector<Token> m_macro_tokens;

    // Macros
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_header(std::vector<Token>& tok);
    Result<std::unique_ptr<NodeMacroDefinition>> parse_macro_definition(std::vector<Token>& tok);
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_call(std::vector<Token>& tok);
    Result<std::unique_ptr<NodeIterateMacro>> parse_iterate_macro(std::vector<Token>& tok);
//	Result<std::unique_ptr<NodeMacroDefinition>&> get_defined_macro(std::unique_ptr<NodeMacroHeader> &macro_call);

    Result<SuccessTag> process_macro_definitions();
    Result<SuccessTag> process_macro_calls(std::vector<Token>& tok);
    bool is_defined_macro(const std::string &name);
    bool is_macro_call(const std::vector<Token>& tok);
    Result<SuccessTag> evaluate_macro_call(std::unique_ptr<NodeMacroHeader> macro_call, std::vector<Token>& tok);

};

class SimpleInterpreter {
public:
    explicit SimpleInterpreter() {}
    Result<int> evaluate_int_expression(std::unique_ptr<NodeAST>& root);
};