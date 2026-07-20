#pragma once

#include <cstddef>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "SourceLocation.h"

class DiagnosticEngine;

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
std::string error_type_to_string(const ErrorType type);


enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4
};

/// A function-call frame that remains valid after the AST traversal has unwound because it will be copied in the event
/// of an error
struct DiagnosticFrame {
    std::string function;
    /// Kept separate from SourceRange so ranges remain cheap to copy on AST nodes.
    std::string file;
    SourceRange call_site;
};

/**
 * A transport object for compiler messages.
 *
 * Diagnostics own all strings required by sinks, so a collecting sink may retain them
 * independently of tokens and AST nodes. The call stack is populated lazily by the
 * active DiagnosticEngine when the diagnostic is emitted.
 */
struct Diagnostic {
    ErrorType type = ErrorType::CompileError;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::string expected;
    std::string actual;
    /// Owning source path corresponding to `range`.
    std::string file;
    SourceRange range;
    std::vector<DiagnosticFrame> call_stack;

    Diagnostic() = default;
    Diagnostic(ErrorType type, std::string message, std::string expected, const struct Token& token);
    Diagnostic(ErrorType type, std::string message, size_t line_number, std::string expected,
               std::string actual, std::string file_name);

    /// Emits a non-fatal diagnostic through the supplied compilation context.
    void report(DiagnosticEngine& diagnostics) const;
    /// Aborts the current compilation by throwing CompilationAborted.
    [[noreturn]] void exit() const;

    void set_message(const std::string& value) { message = value; }
    void add_message(const std::string& value) {
        if (!message.empty()) message += '\n';
        message += value;
    }
    void set_expected(const std::string& value) { expected = value; }
    void set_token(const Token& token);

};

/// Internal control-flow exception used to stop one compilation without terminating the process.
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
