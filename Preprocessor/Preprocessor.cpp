//
// Created by Mathias Vatter on 23.09.23.
//

#include <filesystem>
#include <iostream>
#include "Preprocessor.h"
#include "PreprocessorImport.h"
#include "PreprocessorConditions.h"
#include "PreprocessorParser.h"
#include "PreASTDesugar.h"
#include "PreASTCombine.h"
#include "PreASTIncrementer.h"
#include "PreASTDefines.h"

Preprocessor::Preprocessor(std::vector<Token> tokens, std::string current_file)
    : Parser(std::move(tokens)), m_current_file(std::move(current_file)) {
}

void Preprocessor::process() {
	Result<SuccessTag> result = Result<SuccessTag>(SuccessTag{});

//    PreprocessorImport imports(m_tokens, m_current_file, m_builtin_widgets);
//	result = imports.process_imports();
//	if(result.is_error()) {
//		result.get_error().print();
//		auto err_msg = "Preprocessor failed while processing import statements.";
//		CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek(m_tokens).file).print();
//		exit(EXIT_FAILURE);
//	}
//    m_tokens = std::move(imports.get_tokens());

	PreprocessorConditions conditions(m_tokens, m_current_file);
	result = conditions.process_conditions();
	if(result.is_error()) {
		result.get_error().print();
		auto err_msg = "Preprocessor failed while processing conditions.";
		CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek(m_tokens).file).print();
		exit(EXIT_FAILURE);
	}
	m_tokens = std::move(conditions.get_tokens());

    PreprocessorParser parser(m_tokens);
    auto result_parse = parser.parse_program(nullptr);
    if(result_parse.is_error()) {
        result_parse.get_error().print();
        auto err_msg = "Preprocessor failed.";
        CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek(m_tokens).file).print();
        exit(EXIT_FAILURE);
    }
    auto pre_ast = std::move(result_parse.unwrap());
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

std::vector<Token> Preprocessor::get_tokens() {
    return std::move(m_tokens);
}

Token Preprocessor::peek(const std::vector<Token>& tok, int ahead) {
	if (tok.size() < m_pos+ahead) {
		auto err_msg = "Reached the end of the m_tokens. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).print();
		exit(EXIT_FAILURE);
	}
	m_curr_token = tok.at(m_pos).type;
    m_curr_token_value = tok.at(m_pos).val;
	return tok.at(m_pos+ahead);

}

Token Preprocessor::consume(const std::vector<Token>& tok) {
	if (m_pos < tok.size()) {
		m_curr_token = tok.at(m_pos + 1).type;
        m_curr_token_value = tok.at(m_pos+1).val;
		return tok.at(m_pos++);
	}
	auto err_msg = "Reached the end of the m_tokens. Wrong Syntax discovered.";
	CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).print();
	exit(EXIT_FAILURE);
}

const Token& Preprocessor::get_tok(const std::vector<Token>& tok) const {
	return tok.at(m_pos);
}


void Preprocessor::remove_tokens(std::vector<Token>& tok, size_t start, size_t end) {
    if (start < end && end <= tok.size()) {
        tok.erase(tok.begin() + start, tok.begin() + end);
        // Adjust m_pos if necessary
        if (m_pos > start)
            m_pos -= (end - start);
    } else {
        auto err_msg = "Attempted to remove a token range out of bounds.";
        CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "unknown", tok.at(m_pos).val, tok.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
}

size_t Preprocessor::search(const std::vector<std::string>& vec, const std::string& str) {
	auto it = std::find(vec.begin(), vec.end(), str);
	if (it != vec.end())
		return std::distance(vec.begin(), it);
	return (size_t)-1;
}


std::string Preprocessor::token_vector_to_string(const std::vector<Token>& tokens) {
    std::string output;
    for (auto const & tok : tokens) {
        output += tok.val;
    }
    return output;
}

const std::vector<std::unique_ptr<NodeAST>> &Preprocessor::get_external_variables() const {
    return m_external_variables;
}










