//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreprocessorBuiltins.h"


PreprocessorBuiltins::PreprocessorBuiltins()
        : Parser(std::vector<Token>{}, std::vector<std::string>{}) {
    m_pos = 0;

//    m_curr_token = m_tokens_variables.at(0).type;
}

Result<SuccessTag> PreprocessorBuiltins::process_variables(const std::string &file) {
    Tokenizer tokenizer(file);
    m_tokens_variables = tokenizer.tokenize();
    return Result<SuccessTag>(SuccessTag{});
}
