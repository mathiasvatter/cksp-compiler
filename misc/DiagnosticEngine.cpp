#include "DiagnosticEngine.h"

#include <utility>

void DiagnosticEngine::report(Diagnostic diagnostic) {
    if (diagnostic.call_stack.empty()) diagnostic.call_stack = materialize_call_stack();
    ++m_diagnostic_count;
    m_sink.report(std::move(diagnostic));
}

void DiagnosticEngine::fatal(Diagnostic diagnostic) const {
    diagnostic.severity = DiagnosticSeverity::Error;
    if (diagnostic.call_stack.empty()) diagnostic.call_stack = materialize_call_stack();
    throw CompilationAborted(std::move(diagnostic));
}

void DiagnosticEngine::push_frame(const std::string_view function, const std::string_view file, const SourceRange& call_site) {
    m_call_stack.push_back({function, file, &call_site});
}

void DiagnosticEngine::pop_frame() noexcept {
    if (!m_call_stack.empty()) m_call_stack.pop_back();
}

void DiagnosticEngine::preserve_frame_during_unwind() {
    if (m_call_stack.empty()) return;
    const auto& frame = m_call_stack.back();
    m_unwound_call_stack.push_back({std::string(frame.function), std::string(frame.file), *frame.call_site});
    m_call_stack.pop_back();
}

std::vector<DiagnosticFrame> DiagnosticEngine::materialize_call_stack() const {
    if (!m_unwound_call_stack.empty()) {
        return {m_unwound_call_stack.rbegin(), m_unwound_call_stack.rend()};
    }
    std::vector<DiagnosticFrame> frames;
    frames.reserve(m_call_stack.size());
    for (const auto& frame : m_call_stack) {
        frames.push_back({std::string(frame.function), std::string(frame.file), *frame.call_site});
    }
    return frames;
}
