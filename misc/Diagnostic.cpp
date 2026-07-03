#include "Diagnostic.h"

#include "DiagnosticEngine.h"
#include "DiagnosticSink.h"
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
      actual(std::move(actual)) {
    range.file = std::move(file_name);
    range.start = {line_number, 0};
    range.end = {line_number, 0};
}

void Diagnostic::report() const {
    auto diagnostic = *this;
    diagnostic.severity = DiagnosticSeverity::Warning;
    if (auto* engine = DiagnosticEngine::current()) {
        engine->report(std::move(diagnostic));
        return;
    }
    ConsoleDiagnosticSink sink(std::cout, false);
    sink.report(std::move(diagnostic));
}

void Diagnostic::exit() const {
    auto diagnostic = *this;
    diagnostic.severity = DiagnosticSeverity::Error;
    if (auto* engine = DiagnosticEngine::current()) {
        engine->fatal(std::move(diagnostic));
    }
    throw CompilationAborted(std::move(diagnostic));
}

void Diagnostic::set_token(const Token& token) {
    actual = token.val;
    range = source_range_from_token(token);
}
