//
// Created by Mathias Vatter on 11.10.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorMacros : public Preprocessor {
public:
    PreprocessorMacros(std::vector<Token> tokens, const std::string &currentFile);
    Result<SuccessTag> process_macros();

private:
    std::vector<std::unique_ptr<NodeMacroHeader>> m_macro_iterations;
    std::vector<std::string> m_macro_evaluation_stack;
    std::vector<Token> m_macro_tokens;

	Result<std::unique_ptr<NodeMacroDefinition>> get_macro_definition(Token& macro_name, int num_args);
    // Macros
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_header(std::vector<Token>& tok);
    Result<std::unique_ptr<NodeMacroDefinition>> parse_macro_definition(std::vector<Token>& tok);
    Result<std::unique_ptr<NodeMacroHeader>> parse_macro_call(std::vector<Token>& tok);
	Result<std::vector<std::unique_ptr<NodeMacroHeader>>> parse_iterate_macro(std::vector<Token>& tok);
	Result<std::vector<std::unique_ptr<NodeMacroHeader>>> parse_literate_macro(std::vector<Token>& tok);

    Result<SuccessTag> process_macro_definitions();
    Result<SuccessTag> process_macro_iterations();
    Result<SuccessTag> process_macro_calls(std::vector<Token>& tok);
    bool is_defined_macro(const std::string &name);
    bool is_macro_call(const std::vector<Token>& tok);
	static bool is_replacement_macro(const std::vector<std::vector<Token>>& args);
	bool is_beginning_of_line_keyword(const std::vector<Token>& tok, token token);
    Result<SuccessTag> evaluate_macro_call(std::unique_ptr<NodeMacroHeader> macro_call, std::vector<Token>& tok);

};