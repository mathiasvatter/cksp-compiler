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
        CompileError(ErrorType::PreprocessorError, err_msg, -1, "", "",peek().file).print();
        exit(EXIT_FAILURE);
    }
    m_tokens = std::move(macros.get_tokens());
}

std::vector<Token> Preprocessor::get_tokens() {
    return std::move(m_tokens);
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

void Preprocessor::remove_tokens(size_t start, size_t end) {
    if (start < end && end <= m_tokens.size()) {
        m_tokens.erase(m_tokens.begin() + start, m_tokens.begin() + end);
        // Adjust m_pos if necessary
        if (m_pos > start)
            m_pos -= (end - start);
    } else {
        auto err_msg = "Attempted to remove a token range out of bounds.";
        CompileError(ErrorType::PreprocessorError, err_msg, m_tokens.at(m_pos).line, "unknown", m_tokens.at(m_pos).val, m_tokens.at(m_pos).file).print();
        exit(EXIT_FAILURE);
    }
}



