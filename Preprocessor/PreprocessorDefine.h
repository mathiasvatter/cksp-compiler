//
// Created by Mathias Vatter on 13.10.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorDefine : public Preprocessor {
public:
    PreprocessorDefine(const std::vector<Token> &tokens, const std::string &currentFile);
    Result<SuccessTag> process_defines();

private:
    Result<std::unique_ptr<NodeDefineStatement>> parse_define_statement();

};


