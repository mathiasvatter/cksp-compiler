#include "DiagnosticSink.h"

#include <fstream>

#include "CompileError.h"
#include "../utils/StringUtils.h"

namespace {
const std::string& severity_color(const DiagnosticSeverity severity) {
    return severity == DiagnosticSeverity::Warning ? ColorCode::Yellow : ColorCode::Red;
}

const char* severity_name(const DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Error: return "CompileError";
        case DiagnosticSeverity::Warning: return "CompileWarning";
        case DiagnosticSeverity::Information: return "Information";
    }
    return "Diagnostic";
}
}

void ConsoleDiagnosticSink::report(Diagnostic diagnostic) {
    if (diagnostic.actual == "\n") diagnostic.actual = "linebreak";

    const auto& color = severity_color(diagnostic.severity);
    m_output << color << ColorCode::Bold << severity_name(diagnostic.severity) << ColorCode::Reset;
    m_output << color << " [Type: " << ColorCode::Bold
             << CompileError::error_type_to_string(diagnostic.type) << ColorCode::Reset;
    m_output << color << ", Position: " << diagnostic.range.file;

    if (diagnostic.range.start.line != static_cast<size_t>(-1)) {
        m_output << ':' << diagnostic.range.start.line;
    }
    if (diagnostic.range.start.column > 0) {
        m_output << ':' << diagnostic.range.start.column;
    }
    m_output << "]\n" << diagnostic.message << '\n';

    if (!diagnostic.expected.empty()) {
        m_output << "Expected: '" << StringUtils::normalize_field(diagnostic.expected) << "'; ";
    }
    if (!diagnostic.actual.empty()) {
        m_output << "Got: '" << StringUtils::normalize_field(diagnostic.actual) << "'";
    }
    m_output << ColorCode::Reset << std::endl;

    if (diagnostic.range.start.line != static_cast<size_t>(-1) && !diagnostic.range.file.empty()) {
        std::ifstream file(diagnostic.range.file);
        std::string line;
        for (size_t i = 0; file && i < diagnostic.range.start.line; ++i) {
            std::getline(file, line);
        }
        if (file || !line.empty()) {
            line = CompileError::replace_tabs_with_spaces(line, 1);
            const std::string prefix = "In line " + std::to_string(diagnostic.range.start.line) + ": ";
            m_output << ColorCode::Bold << prefix << ColorCode::Reset << line << '\n';

            if (diagnostic.range.start.column > 0) {
                const auto marker_length = diagnostic.range.end.column > diagnostic.range.start.column
                    ? diagnostic.range.end.column - diagnostic.range.start.column
                    : size_t{1};
                const auto start = prefix.length() + diagnostic.range.start.column - 1;
                m_output << color << std::string(start, ' ')
                         << std::string(marker_length, '^') << ColorCode::Reset << '\n';
            }
        }
    }

    if (m_print_failure_footer && diagnostic.severity == DiagnosticSeverity::Error) {
        m_output << ColorCode::Red << "\nSeems like the compilation exited with a failure."
                 << ColorCode::Reset << std::endl;
        if (!diagnostic.range.file.empty()) {
            CompileError legacy_error(
                diagnostic.type,
                diagnostic.message,
                diagnostic.range.start.line,
                diagnostic.expected,
                diagnostic.actual,
                diagnostic.range.file);
            legacy_error.m_line_position = diagnostic.range.start.column;
            legacy_error.m_marker_length = diagnostic.range.end.column > diagnostic.range.start.column
                ? diagnostic.range.end.column - diagnostic.range.start.column
                : size_t{1};
            m_output << "To help make cksp better, please report any compiler related issues here: "
                     << legacy_error.generate_github_issue_url("mathiasvatter", "cksp-compiler")
                     << std::endl;
        }
    }
}
