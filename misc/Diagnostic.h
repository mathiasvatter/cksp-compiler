#pragma once

#include <cstddef>
#include <exception>
#include <optional>
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
std::string error_type_to_string(ErrorType type);


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
 * Where a word stood before a <#param#> substitution assembled it.
 *
 * A macro body word like <#browser#.foo> becomes <browser.foo>, a name that stands in no
 * file: the substituted half comes from the call site, the rest from the macro body. The
 * token is reported at the call site, which is where the argument came from but not where
 * the offending code is written - without this the message points at <update(browser)> and
 * says nothing about which body line broke.
 */
struct DiagnosticExpansion {
    std::string spelling;  ///< the word as the source spells it, e.g. "#browser#.foo"
    std::string file;
    SourceRange range;

    friend bool operator==(const DiagnosticExpansion&, const DiagnosticExpansion&) = default;
};

/**
 * A transport object for compiler messages.
 *
 * Diagnostics own all strings required by sinks, so a collecting sink may retain them
 * independently of tokens and AST nodes. The call stack is populated lazily by the
 * active DiagnosticEngine when the diagnostic is emitted.
 */
struct Diagnostic {

    enum class MigrationKind {
        SublimePragma,
        Taskfunc,
        TCM,
        PostMacro,
        Property,
        IdentifierCase,
        GlobalDeclarationInitializer,
        ReservedResultName,
        InvalidCharacter
    };

    struct DiagnosticFix {
        enum class FixKind {
            AddRefToFuncParam,
            ConvertDeprecatedFunctionReturn,
            ConvertTaskfuncToFunction,
            ConvertTCMCall,
            CorrectNameCase,
            ConvertSublimePragma,
            SplitGlobalDeclarationAssignment,
            RenameReservedResult,
            ReplaceInvalidCharacter
        };
        enum class EditKind {
            InsertBefore,
            InsertAfter,
            Replace
        };
        struct Edit {
            EditKind kind = EditKind::Replace;
            std::string file;
            SourceRange range;
            std::string new_text;

            friend bool operator==(const Edit&, const Edit&) = default;
        };

        FixKind kind;
        std::string title;
        std::vector<Edit> edits;
        bool is_preferred = false;

        friend bool operator==(const DiagnosticFix&, const DiagnosticFix&) = default;
    };
    static std::string fix_kind_to_string(const DiagnosticFix::FixKind kind) {
        switch (kind) {
            case DiagnosticFix::FixKind::AddRefToFuncParam: return "AddRefToFuncParam";
            case DiagnosticFix::FixKind::ConvertDeprecatedFunctionReturn: return "ConvertDeprecatedFunctionReturn";
            case DiagnosticFix::FixKind::ConvertTaskfuncToFunction: return "ConvertTaskfuncToFunction";
            case DiagnosticFix::FixKind::ConvertTCMCall: return "ConvertTCMCall";
            case DiagnosticFix::FixKind::CorrectNameCase: return "CorrectNameCase";
            case DiagnosticFix::FixKind::ConvertSublimePragma: return "ConvertSublimePragma";
            case DiagnosticFix::FixKind::SplitGlobalDeclarationAssignment: return "SplitGlobalDeclarationAssignment";
            case DiagnosticFix::FixKind::RenameReservedResult: return "RenameReservedResult";
            case DiagnosticFix::FixKind::ReplaceInvalidCharacter: return "ReplaceInvalidCharacter";
            default: break;
        }
        return "unknown";
    }
    static std::string migration_kind_to_string(const MigrationKind kind) {
        switch (kind) {
            case MigrationKind::SublimePragma: return "SublimePragma";
            case MigrationKind::Taskfunc: return "Taskfunc";
            case MigrationKind::TCM: return "TCM";
            case MigrationKind::PostMacro: return "PostMacro";
            case MigrationKind::Property: return "Property";
            case MigrationKind::IdentifierCase: return "IdentifierCase";
            case MigrationKind::GlobalDeclarationInitializer: return "GlobalDeclarationInitializer";
            case MigrationKind::ReservedResultName: return "ReservedResultName";
            case MigrationKind::InvalidCharacter: return "InvalidCharacter";
            default: break;
        }
        return "unknown";
    }

    ErrorType type = ErrorType::CompileError;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string message;
    std::string expected;
    std::string actual;
    /// Owning source path corresponding to `range`.
    std::string file;
    SourceRange range;
    std::vector<DiagnosticFrame> call_stack;
    /// Set when the reported token was assembled by a macro or define substitution.
    std::optional<DiagnosticExpansion> expansion;
    /// Marks a diagnostic about porting a file rather than about the program in it: source
    /// written in another dialect, or a character that came along with it. What they have in
    /// common is that a whole workspace of them can be dealt with in one sweep, which is what
    /// the migration assistant does with them.
    ///
    /// Unlike DiagnosticFix this remains present when the construct cannot be rewritten safely.
    std::optional<MigrationKind> migration_kind;
    std::optional<DiagnosticFix> fix;
    /// Set on an error the compilation carried on past, which tells it apart from the one
    /// that ended the run - the only one a console sink should follow with a failure notice.
    bool recovered = false;

    Diagnostic() = default;
    Diagnostic(ErrorType type, std::string message, std::string expected, const struct Token& token);
    Diagnostic(ErrorType type, std::string message, size_t line_number, std::string expected,
               std::string actual, std::string file_name);

    /// Emits a non-fatal diagnostic through the supplied compilation context.
    void report(DiagnosticEngine& diagnostics) const;
    /// Emits an error the compilation recovered from and goes on past.
    ///
    /// Between <report>, which downgrades to a warning and lets the compile succeed, and
    /// <exit>, which stops at the first message. An error emitted this way is counted by the
    /// engine, and the compiler refuses to generate code while that count is not zero - the
    /// run reaches its end so every further error is found, and still fails.
    void report_as_error(DiagnosticEngine& diagnostics) const;
    /// Aborts the current compilation by throwing CompilationAborted.
    [[noreturn]] void exit() const;

    void set_message(const std::string& value) { message = value; }
    void add_message(const std::string& value) {
        if (!message.empty()) message += '\n';
        message += value;
    }
    void set_expected(const std::string& value) { expected = value; }
    void set_token(const Token& token);
    [[nodiscard]] std::string display_message() const;
    /// "Expected: …" / "Got: …", rendered readable. Empty when neither is known.
    [[nodiscard]] std::string display_detail() const;
    /// The substitution a token came out of, if any. See DiagnosticExpansion.
    [[nodiscard]] static std::optional<DiagnosticExpansion> expansion_of(const struct Token& token);

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
