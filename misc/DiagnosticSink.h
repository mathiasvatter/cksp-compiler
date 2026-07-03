#pragma once

#include <cstddef>
#include <iostream>
#include <ostream>
#include <utility>
#include <vector>

#include "Diagnostic.h"

class DiagnosticSink {
public:
    virtual ~DiagnosticSink() = default;
    virtual void report(Diagnostic diagnostic) = 0;
};

class CollectingDiagnosticSink final : public DiagnosticSink {
public:
    void report(Diagnostic diagnostic) override {
        m_diagnostics.push_back(std::move(diagnostic));
    }

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept {
        return m_diagnostics;
    }

    [[nodiscard]] size_t size() const noexcept {
        return m_diagnostics.size();
    }

private:
    std::vector<Diagnostic> m_diagnostics;
};

class ConsoleDiagnosticSink final : public DiagnosticSink {
public:
    explicit ConsoleDiagnosticSink(std::ostream& output = std::cout, bool print_failure_footer = true)
        : m_output(output), m_print_failure_footer(print_failure_footer) {}

    void report(Diagnostic diagnostic) override;

private:
    std::ostream& m_output;
    bool m_print_failure_footer;
};
