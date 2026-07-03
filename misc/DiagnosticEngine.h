#pragma once

#include <cstddef>
#include <vector>

#include "DiagnosticSink.h"

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(DiagnosticSink& sink) : m_sink(sink) {}

    void report(Diagnostic diagnostic);
    void warning(Diagnostic diagnostic);
    [[noreturn]] void fatal(Diagnostic diagnostic);
    void push_frame(DiagnosticFrame frame);
    void pop_frame() noexcept;

    [[nodiscard]] size_t diagnostic_count() const noexcept {
        return m_diagnostic_count;
    }

    [[nodiscard]] static DiagnosticEngine* current() noexcept;

private:
    DiagnosticSink& m_sink;
    size_t m_diagnostic_count = 0;
    std::vector<DiagnosticFrame> m_call_stack;
};

class DiagnosticFrameScope {
public:
    DiagnosticFrameScope(DiagnosticEngine& engine, DiagnosticFrame frame);
    ~DiagnosticFrameScope();

    DiagnosticFrameScope(const DiagnosticFrameScope&) = delete;
    DiagnosticFrameScope& operator=(const DiagnosticFrameScope&) = delete;

private:
    DiagnosticEngine& m_engine;
};

/// Exposes the active compilation engine to context-free diagnostic construction sites.
class DiagnosticEngineScope {
public:
    explicit DiagnosticEngineScope(DiagnosticEngine& engine) noexcept;
    ~DiagnosticEngineScope();

    DiagnosticEngineScope(const DiagnosticEngineScope&) = delete;
    DiagnosticEngineScope& operator=(const DiagnosticEngineScope&) = delete;

private:
    DiagnosticEngine* m_previous;
};
