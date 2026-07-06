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
#if defined(_WIN32)
#include <windows.h>
#include <VersionHelpers.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <sys/utsname.h>
#endif

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


namespace cli {
    #if defined(__APPLE__) || defined(__linux__)
    inline std::string execute_command(const char* command) {
        std::array<char, 128> buffer{};
        std::string result;
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command, "r"), pclose);
        if (!pipe) return "";
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) result += buffer.data();
        result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
        return result;
    }
    #endif
}

inline std::string get_os_version() {
#if defined(_WIN32)
    if (IsWindows10OrGreater()) return "Windows 10";
    if (IsWindows8Point1OrGreater()) return "Windows 8.1";
    if (IsWindows8OrGreater()) return "Windows 8";
    if (IsWindows7SP1OrGreater()) return "Windows 7 SP1";
    if (IsWindows7OrGreater()) return "Windows 7";
    return "Windows version unknown";
#elif defined(__APPLE__)
    return cli::execute_command("sw_vers -productName") + " " + cli::execute_command("sw_vers -productVersion");
#elif defined(__linux__)
    return execute_command("lsb_release -d");
#else
    return "Unknown OS";
#endif
}

/// Helps getting the current os architecture for the cli error report
inline std::string get_os_architecture() {
#if defined(_WIN32)
    BOOL is_wow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &is_wow64);
    return is_wow64 ? "x64" : "x86";
#elif defined(__APPLE__) || defined(__linux__)
    struct utsname buffer{};
    return uname(&buffer) == -1 ? "unknown" : buffer.machine;
#else
    return "unknown";
#endif
}

inline std::string generate_github_issue_url(const Diagnostic& diagnostic, const std::string& username, const std::string& repo) {
    std::stringstream description;
    description << diagnostic.message;
    if (!diagnostic.expected.empty()) description << "\nExpected: " << diagnostic.expected;
    if (!diagnostic.actual.empty()) description << "\nGot: " << diagnostic.actual;
    if (!diagnostic.file.empty()) description << "\nFile: " << diagnostic.file;
    if (diagnostic.range.start.line != static_cast<size_t>(-1)) {
        description << "\nLine: " << diagnostic.range.start.line;
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