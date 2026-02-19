//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreAST/PreASTNodes/PreAST.h"
#include "../Processor/Processor.h"


class PreprocessorParser : public Processor {
public:
    explicit PreprocessorParser(std::vector<Token> tokens);

    Result<std::unique_ptr<PreNodeProgram>> parse_program(PreNodeAST* parent);

private:
	PreNodeProgram* m_program = nullptr;

    Result<std::unique_ptr<PreNodeNumber>> parse_number(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeAST>> parse_int(PreNodeAST *parent);
    Result<std::unique_ptr<PreNodeKeyword>> parse_keyword(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeOther>> parse_other(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeAST>> parse_token(PreNodeAST* parent);
//    Result<std::unique_ptr<PreNodeChunk>> parse_chunk(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeList>> parse_list(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodePragma>> parse_pragma(PreNodeAST* parent);

    /// DEFINES
    Result<std::unique_ptr<PreNodeDefineHeader>> parse_define_header(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeDefineCall>> parse_define_call(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeDefineStatement>> parse_define_definition(PreNodeAST* parent);

    /// MACROS
    Result<std::unique_ptr<PreNodeMacroHeader>> parse_macro_header(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeMacroCall>> parse_macro_call(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeMacroDefinition>> parse_macro_definition(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeIterateMacro>> parse_iterate_macro(PreNodeAST* parent);
    Result<std::unique_ptr<PreNodeLiterateMacro>> parse_literate_macro(PreNodeAST* parent);

	/// INCREMENTER
    Result<std::unique_ptr<PreNodeIncrementer>> parse_incrementer(PreNodeAST* parent);

	/// IMPORT
	Result<std::unique_ptr<PreNodeImport>> parse_import(PreNodeAST* parent);
	Result<std::unique_ptr<PreNodeImportNCKP>> parse_import_nckp(PreNodeAST* parent);

	/// PREPROCESSOR CONDITIONS
	Result<std::unique_ptr<PreNodeSetCondition>> parse_set_condition(PreNodeAST* parent);
	Result<std::unique_ptr<PreNodeResetCondition>> parse_reset_condition(PreNodeAST* parent);


    std::unordered_map<StringIntKey, std::string, StringIntKeyHash> m_define_strings;
    // macro name and num_macro_arguments
    std::unordered_map<StringIntKey, std::string, StringIntKeyHash> m_macro_strings;
    std::set<std::string> m_macro_iterate_strings;
    bool m_parsing_iterator_macro = false;
	bool m_parsing_literate_macro = false;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> m_define_definitions;

    int get_num_params_in_definition();

    bool is_define_call(const Token &tok);
    bool is_macro_call(const Token &tok);
    bool is_define_definition();
    bool is_macro_definition();
};



