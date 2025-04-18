//
// Created by Mathias Vatter on 23.09.23.
//

#include <iostream>
#include "Preprocessor.h"
#include "PreprocessorConditions.h"
#include "PreprocessorParser.h"
#include "PreAST/PreASTDesugar.h"
#include "PreAST/PreASTCombine.h"
#include "PreAST/PreASTIncrementer.h"
#include "PreAST/PreASTDefines.h"
#include "PreAST/PreASTPragma.h"

Preprocessor::Preprocessor(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {}

void Preprocessor::process() {
	auto result = Result<SuccessTag>(SuccessTag{});

	PreprocessorConditions conditions(m_tokens);
	result = conditions.process_conditions();
	if(result.is_error()) {
		auto error = result.get_error();
		error.m_message += " Preprocessor failed while processing conditions.";
		error.exit();
	}
	m_tokens = std::move(conditions.get_token_vector());

    PreprocessorParser parser(m_tokens);
    auto result_parse = parser.parse_program(nullptr);
    if(result_parse.is_error()) {
        auto error = result_parse.get_error();
        error.m_message += " Preprocessor parsing failed.";
        error.exit();
    }
    auto pre_ast = std::move(result_parse.unwrap());
    PreASTPragma pragma;
    pre_ast->accept(pragma);
    m_output_path = pragma.get_output_path();

	PreASTDefines defines;
	pre_ast->accept(defines);

    PreASTIncrementer incrementer;
    pre_ast->accept(incrementer);

    PreASTDesugar desugar;
    pre_ast->accept(desugar);

    PreASTCombine combine;
    pre_ast->accept(combine);

    m_tokens = std::move(combine.m_tokens);
}

const std::string &Preprocessor::get_output_path() const {
    return m_output_path;
}

std::vector<Token> Preprocessor::get_token_vector() {
	return std::move(m_tokens);
}









