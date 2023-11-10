//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "Preprocessor.h"
#include "PreAST.h"

class PreprocessorParser : public Preprocessor {
public:
    PreprocessorParser(std::vector<Token> tokens, const std::string &currentFile);

    Result<std::unique_ptr<PreNodeProgram>> parse_program(PreNodeAST* parent);
private:
    Result<std::unique_ptr<PreNodeNumber>> parse_number(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeKeyword>> parse_keyword(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeOther>> parse_other(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeAST>> parse_token(PreNodeAST* parent);
//    Result<std::unique_ptr<PreNodeChunk>> parse_chunk(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeList>> parse_list(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeDefineHeader>> parse_define_header(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeDefineStatement>> parse_define_definition(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeDefineCall>> parse_define_call(PreNodeAST* parent);

    std::vector<std::unique_ptr<PreNodeDefineStatement>> m_define_statements;

    bool is_define_call(const Token &tok);
};

