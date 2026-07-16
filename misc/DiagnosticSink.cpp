#include "DiagnosticSink.h"

#include <fstream>

#include "DiagnosticReport.h"
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
        case DiagnosticSeverity::Hint: return "Hint";
    }
    return "Diagnostic";
}
}

void ConsoleDiagnosticSink::report(Diagnostic diagnostic) {
    if (diagnostic.actual == "\n") diagnostic.actual = "linebreak";

    const bool has_location = diagnostic.range.start.line != static_cast<size_t>(-1) && !diagnostic.file.empty();
    const auto& color = severity_color(diagnostic.severity);
    m_output << color << ColorCode::Bold << severity_name(diagnostic.severity) << ColorCode::Reset;
    m_output << color << " [Type: " << ColorCode::Bold
             << error_type_to_string(diagnostic.type) << ColorCode::Reset;
    m_output << color << ", Position: " << diagnostic.file;

    if (diagnostic.range.start.line != static_cast<size_t>(-1)) {
        m_output << ':' << diagnostic.range.start.line;
    }
    if (diagnostic.range.start.column > 0) {
        m_output << ':' << diagnostic.range.start.column;
    }
    m_output << "]\n" << diagnostic.message << '\n';

    if (!diagnostic.expected.empty() || !diagnostic.actual.empty()) {
        if (!diagnostic.expected.empty()) {
            m_output << ColorCode::Bold << "Expected: " << '\'' << StringUtils::normalize_field(diagnostic.expected) << "'\n";
        }
        if (!diagnostic.actual.empty()) {
            m_output << ColorCode::Bold << "Got:      " << '\'' << StringUtils::normalize_field(diagnostic.actual) << "'\n";
        }
    }
    m_output << ColorCode::Reset << std::endl;

    if (has_location) {
        std::ifstream file(diagnostic.file);
        std::string line;
        for (size_t i = 0; file && i < diagnostic.range.start.line; ++i) {
            std::getline(file, line);
        }
        if (file || !line.empty()) {
            line = StringUtils::replace_tabs_with_spaces(line, 1);
            const auto line_number = std::to_string(diagnostic.range.start.line);
            const std::string gutter = std::string(line_number.length(), ' ');

            m_output << ColorCode::Bold << line_number << " | " << ColorCode::Reset << line << '\n';
            if (diagnostic.range.start.column > 0) {
                const auto marker_length =
                    diagnostic.range.end.column > diagnostic.range.start.column
                    ? diagnostic.range.end.column - diagnostic.range.start.column
                    : size_t{1};
                const auto marker_start = diagnostic.range.start.column - 1;
                m_output << color
                         << gutter << " | "
                         << std::string(marker_start, ' ')
                         << std::string(marker_length, '^')
                         << ColorCode::Reset << '\n';
            }
        }
    }

    // print call stack
    if (!diagnostic.call_stack.empty()) {
        m_output << '\n';
        m_output << ColorCode::Bold << "Call stack:" << ColorCode::Reset << '\n';

        size_t index = 0;
        for (auto frame = diagnostic.call_stack.rbegin(); frame != diagnostic.call_stack.rend(); ++frame, ++index) {
            m_output << "  #" << index << ' ' << frame->function << "()";
            if (!frame->file.empty() && frame->call_site.is_valid()) {
                m_output << '\n'
                         << "     at " << frame->file << ':'
                         << frame->call_site.start.line << ':'
                         << frame->call_site.start.column;
            }
            m_output << '\n';
        }
    }
    m_output << '\n';

    if (m_print_failure_footer && diagnostic.severity == DiagnosticSeverity::Error) {
        m_output << ColorCode::Red << "\nSeems like the compilation exited with a failure."
                 << ColorCode::Reset << std::endl;
        if (!diagnostic.file.empty()) {
            m_output << "To help make cksp better, please report any compiler related issues here: "
                     << generate_github_issue_url(diagnostic, "mathiasvatter", "cksp-compiler")
                     << std::endl;
        }
    }
}
