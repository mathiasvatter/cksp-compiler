#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "DiagnosticSink.h"

/**
 * Routes diagnostics to a sink and attaches the active CKSP function-call stack.
 *
 * Active frames are non-owning and therefore cheap on successful compilations. They are
 * converted to owning DiagnosticFrames only when a message is actually emitted.
 */
class DiagnosticEngine {
public:
    explicit DiagnosticEngine(DiagnosticSink& sink) : m_sink(sink) {}

    /// Reports a diagnostic (by calling m_sink.report) and fills an empty call stack from the active traversal.
    void report(Diagnostic diagnostic);
    /// Attaches the active stack and aborts the current compilation.
    [[noreturn]] void fatal(Diagnostic diagnostic) const;

    /// Registers a non-owning frame. All arguments must outlive the matching pop_frame().
    void push_frame(std::string_view function, std::string_view file, const SourceRange& call_site);
    void pop_frame() noexcept;
    /// Copies and removes the top frame while its borrowed AST data is still alive.
    void preserve_frame_during_unwind();

    [[nodiscard]] size_t diagnostic_count() const noexcept {
        return m_diagnostic_count;
    }

private:
    /// References AST data guarded by FunctionCallStackScope.
    struct ActiveFrame {
        std::string_view function;
        std::string_view file;
        const SourceRange* call_site;
    };

    /// Copies the active non-owning frames into a diagnostic-safe representation.
    [[nodiscard]] std::vector<DiagnosticFrame> materialize_call_stack() const;

    DiagnosticSink& m_sink;
    size_t m_diagnostic_count = 0;
    std::vector<ActiveFrame> m_call_stack;
    /// Frames preserved inner-to-outer by RAII destructors during exception unwinding.
    std::vector<DiagnosticFrame> m_unwound_call_stack;
};
