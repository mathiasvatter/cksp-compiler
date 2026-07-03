#include "DiagnosticEngine.h"

#include <utility>

namespace {
thread_local DiagnosticEngine* active_diagnostic_engine = nullptr;
}

void DiagnosticEngine::report(Diagnostic diagnostic) {
    ++m_diagnostic_count;
    m_sink.report(std::move(diagnostic));
}

void DiagnosticEngine::warning(Diagnostic diagnostic) {
    diagnostic.severity = DiagnosticSeverity::Warning;
    report(std::move(diagnostic));
}

void DiagnosticEngine::fatal(Diagnostic diagnostic) {
    diagnostic.severity = DiagnosticSeverity::Error;
    throw CompilationAborted(std::move(diagnostic));
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
