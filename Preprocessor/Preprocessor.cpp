//
// Created by Mathias Vatter on 23.09.23.
//

#include <filesystem>
#include <iostream>
#include "Preprocessor.h"
#include "PreprocessorImport.h"
#include "PreprocessorMacros.h"

Preprocessor::Preprocessor(std::vector<Token> tokens, std::string current_file)
    : Parser(std::move(tokens)), m_current_file(std::move(current_file)) {

}

void Preprocessor::process() {
    PreprocessorImport imports(m_tokens, m_current_file);
    m_tokens = std::move(imports.get_tokens());

    PreprocessorMacros macros(m_tokens, m_current_file);
    auto result = macros.process_macros();
    if(result.is_error()) {
        result.get_error().print();
        auto err_msg = "Preprocessor failed while processing macros.";
        CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek(m_tokens).file).print();
        exit(EXIT_FAILURE);
    }
    m_tokens = std::move(macros.get_tokens());
}

std::vector<Token> Preprocessor::get_tokens() {
    return std::move(m_tokens);
}

Token Preprocessor::peek(const std::vector<Token>& tok, int ahead) {
	if (tok.size() < m_pos+ahead) {
		auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).print();
		exit(EXIT_FAILURE);
	}
	m_curr_token = tok.at(m_pos).type;
	return tok.at(m_pos+ahead);

}

Token Preprocessor::consume(const std::vector<Token>& tok) {
	if (m_pos < tok.size()) {
		m_curr_token = tok.at(m_pos + 1).type;
		return tok.at(m_pos++);
	}
	auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
	CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).print();
	exit(EXIT_FAILURE);
}

const Token& Preprocessor::get_tok(const std::vector<Token>& tok) const {
	return tok.at(m_pos);
}

void Preprocessor::remove_last() {
    if (m_pos > 0 && m_pos <= m_tokens.size()) {
        m_tokens.erase(m_tokens.begin() + m_pos - 1);
        m_pos--;  // Adjust m_pos since we've removed an element before it
    } else {
        auto err_msg = "Attempted to remove a token out of bounds.";
        CompileError(ErrorType::PreprocessorError, err_msg, m_tokens.at(m_pos).line, "unknown", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
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



