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

/// One line of a file, 1-based, with tabs normalized to single spaces so the caret below it
/// lines up with the column numbers the compiler counts in.
std::string read_source_line(const std::string& path, const size_t line_number) {
    std::ifstream file(path);
    std::string line;
    for (size_t i = 0; file && i < line_number; ++i) {
        std::getline(file, line);
    }
    if (!file && line.empty()) return {};
    return StringUtils::replace_tabs_with_spaces(line, 1);
}

/// "12 | <source>" plus a caret under `range`.
void print_snippet(
    std::ostream& output,
    const std::string& path,
    const SourceRange& range,
    const std::string& color) {
    const auto line = read_source_line(path, range.start.line);
    if (line.empty() && range.start.line == static_cast<size_t>(-1)) return;

    const auto line_number = std::to_string(range.start.line);
    const std::string gutter(line_number.length(), ' ');
    output << ColorCode::Bold << line_number << " | " << ColorCode::Reset << line << '\n';
    if (range.start.column == 0) return;

    const auto marker_length = range.end.column > range.start.column
        ? range.end.column - range.start.column
        : size_t{1};
    output << color << gutter << " | "
           << std::string(range.start.column - 1, ' ')
           << std::string(marker_length, '^')
           << ColorCode::Reset << '\n';
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
        print_snippet(m_output, diagnostic.file, diagnostic.range, color);
    }

    // The reported position is where the substitution put the token - the call site that
    // supplied the argument. The line that actually contains the offending code is the one
    // in the macro body, so it is shown too; without it the caret sits on a macro call that
    // looks perfectly fine.
    if (diagnostic.expansion && !diagnostic.expansion->file.empty()) {
        const auto& expansion = *diagnostic.expansion;
        m_output << '\n' << ColorCode::Bold
                 << "Substituted from <" << expansion.spelling << ">"
                 << ColorCode::Reset << '\n'
                 << "  at " << expansion.file << ':' << expansion.range.start.line
                 << ':' << expansion.range.start.column << '\n';
        print_snippet(m_output, expansion.file, expansion.range, color);
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
    // Name the fix the editor would apply, so the command line at least says one exists.
    if (diagnostic.fix && !diagnostic.fix->title.empty()) {
        m_output << ColorCode::Bold << "Suggested fix: " << ColorCode::Reset
                 << diagnostic.fix->title << " (available as a quick fix in the editor)\n";
    }
    m_output << '\n';

    if (m_print_failure_footer && diagnostic.severity == DiagnosticSeverity::Error) {
        m_output << ColorCode::Red << "\nSeems like the compilation exited with a failure."
                 << ColorCode::Reset << std::endl;
        // A diagnostic that carries its own fix knows exactly what is wrong and how to undo
        // it, so it is never the unknown compiler problem the issue tracker is asking for.
        if (!diagnostic.file.empty() && !diagnostic.fix) {
            m_output << "To help make cksp better, please report any compiler related issues here: "
                     << generate_github_issue_url(diagnostic, "mathiasvatter", "cksp-compiler")
                     << std::endl;
        }
    }
}
