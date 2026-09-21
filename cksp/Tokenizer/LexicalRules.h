//
// Character-level CKSP lexical rules shared by the compiler tokenizer and live-buffer LSP scans.
//

#pragma once

#include <string_view>

namespace cksp::lexical {

inline constexpr std::string_view VARIABLE_SIGILS = "$~@";
inline constexpr std::string_view ARRAY_SIGILS = "%?!";
inline constexpr char OPEN_PARENTHESIS = '(';
inline constexpr char CLOSE_PARENTHESIS = ')';
inline constexpr char OPEN_BRACKET = '[';
inline constexpr char CLOSE_BRACKET = ']';

[[nodiscard]] inline bool is_ascii_letter(const char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

[[nodiscard]] inline bool is_ascii_digit(const char c) {
	return c >= '0' && c <= '9';
}

[[nodiscard]] inline bool is_identifier_body(const char c) {
	return is_ascii_letter(c) || is_ascii_digit(c) || c == '_' || c == '#';
}

/// KSP type sigils may prefix a name, but may not occur in its body.
[[nodiscard]] inline bool is_identifier_sigil(const char c) {
	return VARIABLE_SIGILS.find(c) != std::string_view::npos
		|| ARRAY_SIGILS.find(c) != std::string_view::npos;
}

[[nodiscard]] inline bool is_identifier_start(const char c) {
	return is_ascii_letter(c) || is_ascii_digit(c) || c == '_' || is_identifier_sigil(c);
}

[[nodiscard]] inline bool is_horizontal_space(const char c) {
	return c == '\t' || c == '\v' || c == '\f' || c == '\r' || c == ' ';
}

[[nodiscard]] inline bool is_string_delimiter(const char c) {
	return c == '\'' || c == '"';
}

[[nodiscard]] inline bool delimiters_match(const char open, const char close) {
	return (open == OPEN_PARENTHESIS && close == CLOSE_PARENTHESIS)
		|| (open == OPEN_BRACKET && close == CLOSE_BRACKET);
}

}
