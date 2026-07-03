#pragma once

#include <cstddef>
#include <exception>
#include <string>
#include <utility>

#include "SourceLocation.h"

enum class ErrorType {
    CompileError,
    CompileWarning,
    FileError,
    SyntaxError,
    TypeError,
    VariableError,
    TokenError,
    ParseError,
    PreprocessorError,
    MathError,
    InternalError
};

enum class DiagnosticSeverity {
    Error,
    Warning,
    Information
};

struct Diagnostic {
    ErrorType type = ErrorType::CompileError;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::string expected;
    std::string actual;
    SourceRange range;
};

class CompilationAborted final : public std::exception {
public:
    explicit CompilationAborted(Diagnostic diagnostic)
        : m_diagnostic(std::move(diagnostic)) {}

    [[nodiscard]] const Diagnostic& diagnostic() const noexcept {
        return m_diagnostic;
    }

    [[nodiscard]] const char* what() const noexcept override {
        return "CKSP compilation aborted";
    }

private:
    Diagnostic m_diagnostic;
};

struct CompilationResult {
    bool success = false;
    size_t diagnostic_count = 0;
};
