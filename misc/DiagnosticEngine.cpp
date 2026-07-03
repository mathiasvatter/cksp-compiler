#include "DiagnosticEngine.h"

#include <utility>

namespace {
thread_local DiagnosticEngine* active_diagnostic_engine = nullptr;
}

void DiagnosticEngine::report(Diagnostic diagnostic) {
    if (diagnostic.call_stack.empty()) diagnostic.call_stack = m_call_stack;
    ++m_diagnostic_count;
    m_sink.report(std::move(diagnostic));
}

void DiagnosticEngine::warning(Diagnostic diagnostic) {
    diagnostic.severity = DiagnosticSeverity::Warning;
    report(std::move(diagnostic));
}

void DiagnosticEngine::fatal(Diagnostic diagnostic) {
    diagnostic.severity = DiagnosticSeverity::Error;
    if (diagnostic.call_stack.empty()) diagnostic.call_stack = m_call_stack;
    throw CompilationAborted(std::move(diagnostic));
}

void DiagnosticEngine::push_frame(DiagnosticFrame frame) {
    m_call_stack.push_back(std::move(frame));
}

void DiagnosticEngine::pop_frame() noexcept {
    if (!m_call_stack.empty()) m_call_stack.pop_back();
}

DiagnosticEngine* DiagnosticEngine::current() noexcept {
    return active_diagnostic_engine;
}

DiagnosticEngineScope::DiagnosticEngineScope(DiagnosticEngine& engine) noexcept
    : m_previous(active_diagnostic_engine) {
    active_diagnostic_engine = &engine;
}

DiagnosticEngineScope::~DiagnosticEngineScope() {
    active_diagnostic_engine = m_previous;
}

DiagnosticFrameScope::DiagnosticFrameScope(DiagnosticEngine& engine, DiagnosticFrame frame)
    : m_engine(engine) {
    m_engine.push_frame(std::move(frame));
}

DiagnosticFrameScope::~DiagnosticFrameScope() {
    m_engine.pop_frame();
}
