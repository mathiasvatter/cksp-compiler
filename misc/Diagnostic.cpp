#include "Diagnostic.h"

#include "DiagnosticEngine.h"
#include "../cksp/Tokenizer/Token.h"
#include "../utils/StringUtils.h"

std::string error_type_to_string(const ErrorType type) {
    switch (type) {
        case ErrorType::CompileError: return "CompileError";
        case ErrorType::CompileWarning: return "CompileWarning";
        case ErrorType::FileError: return "FileError";
        case ErrorType::SyntaxError: return "SyntaxError";
        case ErrorType::TypeError: return "TypeError";
        case ErrorType::VariableError: return "VariableError";
        case ErrorType::TokenError: return "TokenError";
        case ErrorType::ParseError: return "ParseError";
        case ErrorType::PreprocessorError: return "PreprocessorError";
        case ErrorType::MathError: return "MathError";
        case ErrorType::InternalError: return "InternalError";
    }
    return "UnknownError";
}

Diagnostic::Diagnostic(
    const ErrorType type,
    std::string message,
    std::string expected,
    const Token& token)
    : type(type),
      severity(type == ErrorType::CompileWarning ? DiagnosticSeverity::Warning : DiagnosticSeverity::Error),
      message(std::move(message)),
      expected(std::move(expected)),
      actual(token.val),
      file(token.file),
      range(source_range_from_token(token)),
      expansion(expansion_of(token)) {}

std::optional<DiagnosticExpansion> Diagnostic::expansion_of(const Token& token) {
    if (!token.origin || token.origin->file.empty()) return std::nullopt;
    return DiagnosticExpansion{
        token.origin->val, token.origin->file, source_range_from_token(*token.origin)};
}

Diagnostic::Diagnostic(
    const ErrorType type,
    std::string message,
    const size_t line_number,
    std::string expected,
    std::string actual,
    std::string file_name)
    : type(type),
      severity(type == ErrorType::CompileWarning ? DiagnosticSeverity::Warning : DiagnosticSeverity::Error),
      message(std::move(message)),
      expected(std::move(expected)),
      actual(std::move(actual)),
      file(std::move(file_name)) {
    range.start = {line_number, 0};
    range.end = {line_number, 0};
}

void Diagnostic::report(DiagnosticEngine& diagnostics) const {
    auto diagnostic = *this;
    diagnostic.severity = DiagnosticSeverity::Warning;
    diagnostics.report(std::move(diagnostic));
}

void Diagnostic::exit() const {
    auto diagnostic = *this;
    diagnostic.severity = DiagnosticSeverity::Error;
    throw CompilationAborted(std::move(diagnostic));
}

void Diagnostic::set_token(const Token& token) {
    actual = token.val;
    file = token.file;
    range = source_range_from_token(token);
    expansion = expansion_of(token);
}

std::string Diagnostic::display_detail() const {
    // A linebreak is what the parser meets on nearly every unfinished line, and printing it
    // raw leaves the reader with an empty pair of quotes.
    const auto readable = [](const std::string& field) {
        if (field == "\n") return std::string("linebreak");
        if (field.empty()) return std::string("end of file");
        return StringUtils::normalize_field(field);
    };

    std::string detail;
    if (!expected.empty()) detail += "Expected: " + readable(expected);
    if (!actual.empty() || !expected.empty()) {
        if (!detail.empty()) detail += '\n';
        detail += "Got: " + readable(actual);
    }
    return detail;
}

std::string Diagnostic::display_message() const {
    if (!message.empty()) {
        return message;
    }
    auto result = error_type_to_string(type);
    if (!actual.empty()) {
        result += ": " + actual;
    }
    return result;
}
