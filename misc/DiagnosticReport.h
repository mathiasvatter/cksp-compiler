#pragma once

#include <string>

#include "Diagnostic.h"

struct ColorCode {
    inline static const std::string Red = "\033[31m";
    inline static const std::string Green = "\033[32m";
    inline static const std::string Yellow = "\033[33m";
    inline static const std::string Reset = "\033[0m";
    inline static const std::string Bold = "\033[1m";
};

[[nodiscard]] std::string error_type_to_string(ErrorType type);
[[nodiscard]] std::string replace_tabs_with_spaces(const std::string& input, int spaces_per_tab = 4);
[[nodiscard]] std::string get_line_from_file(const Diagnostic& diagnostic);
[[nodiscard]] std::string get_os_version();
[[nodiscard]] std::string get_os_architecture();
[[nodiscard]] std::string generate_github_issue_url(
    const Diagnostic& diagnostic,
    const std::string& username,
    const std::string& repo);
