//
// Created for LSP completion support.
//

#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lsp {

/**
 * Extracts the qualifier chain the cursor sits behind.
 *
 * Completion is requested while the buffer is mid-edit, so the AST snapshot
 * cannot be asked what is left of the cursor — the text can be, and must be.
 * `audio.mixer.|` yields {"audio", "mixer"}.
 *
 * Returns nullopt when there is no qualifier to complete: the character before
 * the cursor is not a dot, the dot is not preceded by an identifier, or the
 * position sits inside a string or a comment.
 *
 * Only the cursor's own line is inspected. A qualifier chain never spans lines,
 * but a `{ ... }` block comment opened on an earlier line is not seen; the
 * caller's snapshot has no tokens to consult while the buffer is broken.
 */
[[nodiscard]] inline std::optional<std::vector<std::string>> qualifier_chain_at(
	const std::string_view line, const size_t character) {
	if (character == 0 || character > line.size()) return std::nullopt;

	const auto is_identifier_char = [](const char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
			|| (c >= '0' && c <= '9') || c == '_';
	};

	// Reject positions inside a string literal or a comment. Both are scanned
	// left to right because their opening delimiter decides the state.
	bool in_string = false;
	bool in_comment = false;
	for (size_t i = 0; i < character; ++i) {
		const char c = line[i];
		if (in_comment) {
			if (c == '}') in_comment = false;
			continue;
		}
		if (in_string) {
			if (c == '"') in_string = false;
			continue;
		}
		if (c == '"') {
			in_string = true;
		} else if (c == '{') {
			in_comment = true;
		} else if (c == '/' && i + 1 < character && line[i + 1] == '/') {
			return std::nullopt;  // rest of the line is a comment
		}
	}
	if (in_string || in_comment) return std::nullopt;

	if (line[character - 1] != '.') return std::nullopt;

	// Walk backwards over `identifier ('[' index ']')? ('.' ...)*` left of the dot.
	std::vector<std::string> chain;
	size_t end = character - 1;  // one past the last identifier character
	while (true) {
		// An indexed element has the type of its elements, so the subscript is skipped
		// and the chain continues at the array's name: <zones[0].> completes a Zone.
		if (end > 0 && line[end - 1] == ']') {
			size_t depth = 0;
			size_t cursor = end;
			while (cursor > 0) {
				--cursor;
				if (line[cursor] == ']') ++depth;
				else if (line[cursor] == '[' && --depth == 0) break;
			}
			if (depth != 0 || line[cursor] != '[') return std::nullopt;
			end = cursor;
		}
		size_t start = end;
		while (start > 0 && is_identifier_char(line[start - 1])) --start;
		if (start == end) return std::nullopt;  // empty segment: `.` or `).`
		chain.emplace_back(line.substr(start, end - start));
		if (start == 0 || line[start - 1] != '.') break;
		end = start - 1;
	}

	// A leading digit means this was a number, not an identifier.
	if (!chain.empty() && !chain.back().empty()
		&& chain.back()[0] >= '0' && chain.back()[0] <= '9') {
		return std::nullopt;
	}

	std::ranges::reverse(chain);
	return chain;
}

/// Convenience wrapper: picks the zero-based `line` out of `text` first.
[[nodiscard]] inline std::optional<std::vector<std::string>> qualifier_chain_in(
	const std::string_view text, const size_t line, const size_t character) {
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
	return qualifier_chain_at(content, character);
}

}
