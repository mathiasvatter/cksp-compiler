#pragma once

#include <string>

#include "Diagnostic.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include "version.h"
#include "../utils/StringUtils.h"

/**
 * Helper functions and structs to print error diagnostics to the console
 */
struct ColorCode {
    inline static const std::string Red = "\033[31m";
    inline static const std::string Green = "\033[32m";
    inline static const std::string Yellow = "\033[33m";
    inline static const std::string Reset = "\033[0m";
    inline static const std::string Bold = "\033[1m";
};


/// Returns "" if diagnostic does not provide a line or a file, otherwise returns the line of the provided range
/// in the provided file
inline std::string get_line_from_file(const Diagnostic& diagnostic) {
    if (diagnostic.file.empty() || diagnostic.range.start.line == static_cast<size_t>(-1)) return "";
    std::ifstream file(diagnostic.file);
    std::string line;
    for (size_t i = 0; file && i < diagnostic.range.start.line; ++i) std::getline(file, line);
    return line;
}


/// The host OS and its architecture, for the report the bug template is prefilled with.
/// Defined in DiagnosticReport.cpp so that <windows.h> stays out of every translation unit that
/// includes this header: it declares the enumerators of TOKEN_INFORMATION_CLASS at global scope,
/// <TokenOrigin> among them, which hides the struct of that name in Token.h.
[[nodiscard]] std::string get_os_version();
[[nodiscard]] std::string get_os_architecture();

inline std::string generate_github_issue_url(const Diagnostic& diagnostic, const std::string& username, const std::string& repo) {
    std::stringstream description;
    description << diagnostic.message;
    if (!diagnostic.expected.empty()) description << "\nExpected: " << diagnostic.expected;
    if (!diagnostic.actual.empty()) description << "\nGot: " << diagnostic.actual;
    if (!diagnostic.file.empty()) description << "\nFile: " << diagnostic.file;
    if (diagnostic.range.start.line != static_cast<size_t>(-1)) {
        description << "\nLine: " << diagnostic.range.start.line;
    }
    if (diagnostic.expansion && !diagnostic.expansion->file.empty()) {
        description << "\nExpanded from: <" << diagnostic.expansion->spelling << "> ("
                    << diagnostic.expansion->file << ':'
                    << diagnostic.expansion->range.start.line << ':'
                    << diagnostic.expansion->range.start.column << ')';
    }
    if (!diagnostic.call_stack.empty()) {
        description << "\n\nCKSP call stack:";
        for (auto frame = diagnostic.call_stack.rbegin(); frame != diagnostic.call_stack.rend(); ++frame) {
            description << "\n  called from " << frame->function;
            if (!frame->file.empty() && frame->call_site.is_valid()) {
                description << " (" << frame->file << ':'
                            << frame->call_site.start.line << ':'
                            << frame->call_site.start.column << ')';
            }
        }
    }

    std::stringstream reproduce;
    reproduce << "```cksp\n" << get_line_from_file(diagnostic) << "\n```";

    auto url = "https://github.com/" + username + "/" + repo + "/issues/new";
    url += "?template=" + StringUtils::percent_encode_uri("bug_report.yml");
    url += "&title=" + StringUtils::percent_encode_uri(
        "[BUG] " + error_type_to_string(diagnostic.type) + ": " + diagnostic.message);
    url += "&description=" + StringUtils::percent_encode_uri(description.str());
    url += "&reproduce=" + StringUtils::percent_encode_uri(reproduce.str());
    url += "&cksp_version=" + StringUtils::percent_encode_uri(COMPILER_VERSION);
    url += "&os=" + StringUtils::percent_encode_uri(get_os_version());
    url += "&arch=" + StringUtils::percent_encode_uri(get_os_architecture());
    return url;
}