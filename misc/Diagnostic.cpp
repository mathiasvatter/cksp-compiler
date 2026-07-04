#include "Diagnostic.h"

#include "DiagnosticEngine.h"
#include "../cksp/Tokenizer/TokenSourceRange.h"

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
      range(source_range_from_token(token)) {}

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
}
