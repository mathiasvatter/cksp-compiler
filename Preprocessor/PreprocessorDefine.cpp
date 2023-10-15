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
    m_pos = 0;
    // parse defines
    while (peek(m_tokens).type != token::END_TOKEN) {
        if(peek(m_tokens).type == token::DEFINE) {
            auto define = parse_define_statement();
            if(define.is_error())
                return Result<SuccessTag>(define.get_error());
            m_define_statements.push_back(std::move(define.unwrap()));
        } else {
            consume(m_tokens);
        }
    }

    return Result<SuccessTag>(SuccessTag{});
}

Result<std::unique_ptr<NodeDefineStatement>> PreprocessorDefine::parse_define_statement() {
    consume(m_tokens); //consume define

}
