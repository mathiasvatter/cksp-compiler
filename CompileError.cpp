//
// Created by Mathias Vatter on 29.08.23.
//

#include "CompileError.h"
#include "Tokenizer/Tokenizer.h"

#include <utility>

CompileError::CompileError(ErrorType type, std::string message, std::string expected, const Token &token)
        : m_type(type), message(std::move(message)), expected(std::move(expected)) {
    line_number = token.line; file_name = token.file; got = token.val;
}
