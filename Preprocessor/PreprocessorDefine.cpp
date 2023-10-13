//
// Created by Mathias Vatter on 13.10.23.
//

#include "PreprocessorDefine.h"

PreprocessorDefine::PreprocessorDefine(const std::vector<Token> &tokens, const std::string &currentFile) : Preprocessor(
        tokens, currentFile) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
}

Result<SuccessTag> PreprocessorDefine::process_defines() {
    return Result<SuccessTag>(SuccessTag{});
}
