//
// Shared source-text primitives for LSP queries that must work on an unfinished buffer.
//

#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../cksp/Tokenizer/LexicalRules.h"

namespace lsp::source_text {

[[nodiscard]] inline bool is_identifier_char(const char c) {
	return cksp::lexical::is_identifier_body(c) || cksp::lexical::is_identifier_sigil(c);
}

[[nodiscard]] inline bool is_horizontal_space(const char c) {
	return cksp::lexical::is_horizontal_space(c);
}

struct LexicalState {
	char string_delimiter = 0;
	size_t brace_comment_depth = 0;
	char block_comment_closing = 0;
	bool line_comment = false;

	[[nodiscard]] bool in_string() const { return string_delimiter != 0; }
	[[nodiscard]] bool in_comment() const {
		return brace_comment_depth > 0 || block_comment_closing != 0 || line_comment;
	}
	[[nodiscard]] bool outside_code() const { return in_string() || in_comment(); }
};

/**
 * Consumes lexical regions whose punctuation is not program structure: strings and comments.
 * `position` may advance over the second byte of a two-character delimiter or an escaped quote.
 * Returns true when the character belongs to such a region and the caller should not inspect it.
 */
[[nodiscard]] inline bool consume_non_code(
	const std::string_view text, size_t& position, LexicalState& state) {
	const char c = text[position];
	if (state.line_comment) {
		if (c == '\n') state.line_comment = false;
		return true;
	}
	if (state.brace_comment_depth > 0) {
		if (c == '{') ++state.brace_comment_depth;
		else if (c == '}') --state.brace_comment_depth;
		return true;
	}
	if (state.block_comment_closing != 0) {
		if (c == '*' && position + 1 < text.size() && text[position + 1] == state.block_comment_closing) {
			state.block_comment_closing = 0;
			++position;
		}
		return true;
	}
	if (state.in_string()) {
		if (c == '\\' && position + 1 < text.size()
			&& text[position + 1] == state.string_delimiter) {
			++position;
		} else if (c == state.string_delimiter) {
			state.string_delimiter = 0;
		}
		return true;
	}

	if (cksp::lexical::is_string_delimiter(c)) {
		state.string_delimiter = c;
		return true;
	}
	if (c == '{') {
		state.brace_comment_depth = 1;
		return true;
	}
	if (c == '(' && position + 1 < text.size() && text[position + 1] == '*') {
		state.block_comment_closing = ')';
		++position;
		return true;
	}
	if (c == '/' && position + 1 < text.size()) {
		if (text[position + 1] == '/') {
			state.line_comment = true;
			++position;
			return true;
		}
		if (text[position + 1] == '*') {
			state.block_comment_closing = '/';
			++position;
			return true;
		}
	}
	return false;
}

[[nodiscard]] inline LexicalState lexical_state_at(
	const std::string_view text, const size_t end) {
	LexicalState state;
	for (size_t position = 0; position < end && position < text.size(); ++position) {
		(void)consume_non_code(text, position, state);
	}
	return state;
}

/**
 * Reads an access chain ending immediately before `end` (an exclusive byte offset).
 *
 * Both completion and signature help need the same source-level spelling rather than an
 * AST node: the live editor buffer is commonly one token away from parsing. Indexed
 * receivers keep the type of their element, so `zones[0].ping` becomes
 * `{ "zones", "ping" }`. Horizontal whitespace around access operators is ignored.
 */
[[nodiscard]] inline std::optional<std::vector<std::string>> access_chain_ending_at(
	const std::string_view text, size_t end) {
	if (end > text.size()) return std::nullopt;

	std::vector<std::string> chain;
	while (true) {
		while (end > 0 && is_horizontal_space(text[end - 1])) --end;

		// An indexed receiver has the type of its element. Skip the subscript and
		// continue at the declaration name: `zones[0].ping` -> `zones.ping`.
		if (end > 0 && text[end - 1] == ']') {
			size_t depth = 0;
			size_t cursor = end;
			while (cursor > 0) {
				--cursor;
				if (text[cursor] == ']') {
					++depth;
				} else if (text[cursor] == '[' && --depth == 0) {
					break;
				}
			}
			if (depth != 0 || text[cursor] != '[') return std::nullopt;
			end = cursor;
			while (end > 0 && is_horizontal_space(text[end - 1])) --end;
		}

		const size_t segment_end = end;
		while (end > 0 && cksp::lexical::is_identifier_body(text[end - 1])) --end;
		if (end == segment_end) return std::nullopt;
		if (end > 0 && cksp::lexical::is_identifier_sigil(text[end - 1])) --end;
		const auto segment = text.substr(end, segment_end - end);
		const size_t first_body = cksp::lexical::is_identifier_sigil(segment.front()) ? 1 : 0;
		if (first_body == segment.size()
			|| cksp::lexical::is_ascii_digit(segment[first_body])) return std::nullopt;
		chain.emplace_back(segment);

		while (end > 0 && is_horizontal_space(text[end - 1])) --end;
		if (end == 0 || text[end - 1] != '.') break;
		--end;
	}

	std::ranges::reverse(chain);
	return chain;
}

/// The zero-based `line` of `text`, without its line ending.
[[nodiscard]] inline std::optional<std::string_view> line_at(
	const std::string_view text, const size_t line) {
	size_t offset = 0;
	for (size_t current = 0; current < line; ++current) {
		const auto newline = text.find('\n', offset);
		if (newline == std::string_view::npos) return std::nullopt;
		offset = newline + 1;
	}
	auto end = text.find('\n', offset);
	if (end == std::string_view::npos) end = text.size();
	auto content = text.substr(offset, end - offset);
	if (!content.empty() && content.back() == '\r') content.remove_suffix(1);
	return content;
}

/// Converts a zero-based LSP line/character pair to an offset in the current buffer.
[[nodiscard]] inline std::optional<size_t> offset_at(
	const std::string_view text, const size_t line, const size_t character) {
	size_t offset = 0;
	for (size_t current = 0; current < line; ++current) {
		const auto newline = text.find('\n', offset);
		if (newline == std::string_view::npos) return std::nullopt;
		offset = newline + 1;
	}
	const auto newline = text.find('\n', offset);
	const auto line_end = newline == std::string_view::npos ? text.size() : newline;
	if (character > line_end - offset) return std::nullopt;
	return offset + character;
}

}
