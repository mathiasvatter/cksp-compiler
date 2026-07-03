#include "DiagnosticEngine.h"

#include <utility>

namespace {
thread_local DiagnosticEngine* active_diagnostic_engine = nullptr;
}

void DiagnosticEngine::report(Diagnostic diagnostic) {
    if (diagnostic.call_stack.empty()) diagnostic.call_stack = materialize_call_stack();
    ++m_diagnostic_count;
    m_sink.report(std::move(diagnostic));
}

void DiagnosticEngine::warning(Diagnostic diagnostic) {
    diagnostic.severity = DiagnosticSeverity::Warning;
    report(std::move(diagnostic));
}

void DiagnosticEngine::fatal(Diagnostic diagnostic) {
    diagnostic.severity = DiagnosticSeverity::Error;
    if (diagnostic.call_stack.empty()) diagnostic.call_stack = materialize_call_stack();
    throw CompilationAborted(std::move(diagnostic));
}

void DiagnosticEngine::push_frame(
    const std::string_view function,
    const std::string_view file,
    const SourceRange& call_site) {
    m_call_stack.push_back({function, file, &call_site});
}

void DiagnosticEngine::pop_frame() noexcept {
    if (!m_call_stack.empty()) m_call_stack.pop_back();
}

DiagnosticEngine* DiagnosticEngine::current() noexcept {
    return active_diagnostic_engine;
}

std::vector<DiagnosticFrame> DiagnosticEngine::materialize_call_stack() const {
    std::vector<DiagnosticFrame> frames;
    frames.reserve(m_call_stack.size());
    for (const auto& frame : m_call_stack) {
        frames.push_back({std::string(frame.function), std::string(frame.file), *frame.call_site});
    }
    return frames;
}

DiagnosticEngineScope::DiagnosticEngineScope(DiagnosticEngine& engine) noexcept
    : m_previous(active_diagnostic_engine) {
    active_diagnostic_engine = &engine;
}

DiagnosticEngineScope::~DiagnosticEngineScope() {
    active_diagnostic_engine = m_previous;
}
