#pragma once

#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

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

struct DiagnosticFrame {
    std::string function;
    SourceRange call_site;
};

struct Diagnostic {
    ErrorType type = ErrorType::CompileError;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::string expected;
    std::string actual;
    SourceRange range;
    std::vector<DiagnosticFrame> call_stack;

    Diagnostic() = default;
    Diagnostic(ErrorType type, std::string message, std::string expected, const struct Token& token);
    Diagnostic(ErrorType type, std::string message, size_t line_number, std::string expected,
               std::string actual, std::string file_name);

    void report() const;
    [[noreturn]] void exit() const;

    void set_message(const std::string& value) { message = value; }
    void add_message(const std::string& value) {
        if (!message.empty()) message += '\n';
        message += value;
    }
    void set_expected(const std::string& value) { expected = value; }
    void set_token(const Token& token);
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
