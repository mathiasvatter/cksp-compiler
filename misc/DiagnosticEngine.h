#pragma once

#include <cstddef>

#include "DiagnosticSink.h"

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(DiagnosticSink& sink) : m_sink(sink) {}

    void report(Diagnostic diagnostic);
    void warning(Diagnostic diagnostic);
    [[noreturn]] void fatal(Diagnostic diagnostic);

    [[nodiscard]] size_t diagnostic_count() const noexcept {
        return m_diagnostic_count;
    }

    [[nodiscard]] static DiagnosticEngine* current() noexcept;

private:
    DiagnosticSink& m_sink;
    size_t m_diagnostic_count = 0;
};

/// Temporarily exposes an engine to legacy compiler code on the current thread.
class DiagnosticEngineScope {
public:
    explicit DiagnosticEngineScope(DiagnosticEngine& engine) noexcept;
    ~DiagnosticEngineScope();

    DiagnosticEngineScope(const DiagnosticEngineScope&) = delete;
    DiagnosticEngineScope& operator=(const DiagnosticEngineScope&) = delete;

private:
    DiagnosticEngine* m_previous;
};
