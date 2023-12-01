//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorConditions : public Preprocessor {
public:
    PreprocessorConditions(std::vector<Token> tokens, const std::string &currentFile);
    Result<SuccessTag> process_conditions();

private:

    bool is_beginning_of_line_keyword(const std::vector<Token>& tok, token token);
    bool is_builtin_condition(const std::vector<Token>& tok, const Token& token);
    bool is_condition_definition(const std::vector<Token>& tok, const Token& peek, token token_type);
	void reset_condition(const Token& condition);

    std::vector<std::string> builtin_conditions = {"NO_SYS_SCRIPT_GROUP_START", "NO_SYS_SCRIPT_PEDAL", "NO_SYS_SCRIPT_RLS_TRIG"};

    Result<SuccessTag> get_conditions();
	Result<SuccessTag> evaluate_conditions();
    Result<Token> parse_condition_definition(std::vector<Token>& tok);
	/// returns true if the following tokens should be evaluated
	Result<bool> parse_use_code_if(std::vector<Token>& tok);
	Result<SuccessTag> parse_end_use_code(std::vector<Token>& tok);
    std::vector<Token> m_conditions;
    std::vector<Token> m_macro_tokens;
};


