//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorBuiltins : public Parser {
public:
    explicit PreprocessorBuiltins();
    Result<SuccessTag> process_variables(const std::string &file);
    Result<std::unique_ptr<NodeAST>> parse_variables();

    std::vector<Token> m_tokens_variables;
};

