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

/// What the text before the cursor asks for.
enum class CompletionContext {
	/// Inside a string or comment, or behind something that is not a name.
	None,
	/// A bare identifier is being typed: everything visible at this position.
	Unqualified,
	/// Behind `a.b.`: the members of that chain.
	Qualified,
};

struct CompletionQuery {
	CompletionContext context = CompletionContext::None;
	std::vector<std::string> chain;
};

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

	// Skip the partial identifier being typed, so completion keeps working when the
	// request arrives mid-word (`audio.ra|`) and not only right behind the dot.
	size_t dot = character;
	while (dot > 0 && is_identifier_char(line[dot - 1])) --dot;
	if (dot == 0 || line[dot - 1] != '.') return std::nullopt;

	// Walk backwards over `identifier ('[' index ']')? ('.' ...)*` left of the dot.
	std::vector<std::string> chain;
	size_t end = dot - 1;  // one past the last identifier character
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

/// True when the position sits inside a string literal or a comment on its line.
[[nodiscard]] inline bool in_string_or_comment(const std::string_view line, const size_t character) {
	bool in_string = false;
	bool in_comment = false;
	for (size_t i = 0; i < character && i < line.size(); ++i) {
		const char c = line[i];
		if (in_comment) {
			if (c == '}') in_comment = false;
			continue;
		}
		if (in_string) {
			if (c == '"') in_string = false;
			continue;
		}
		if (c == '"') in_string = true;
		else if (c == '{') in_comment = true;
		else if (c == '/' && i + 1 < character && line[i + 1] == '/') return true;
	}
	return in_string || in_comment;
}

/// Classifies a position: nothing, a bare identifier, or a qualified chain.
[[nodiscard]] inline CompletionQuery completion_query_at(
	const std::string_view line, const size_t character) {
	if (character > line.size() || in_string_or_comment(line, character)) return {};
	if (auto chain = qualifier_chain_at(line, character)) {
		return {CompletionContext::Qualified, std::move(*chain)};
	}
	// Behind a dot that leads nowhere (`1.`, `abs(1).`) nothing should be offered;
	// anywhere else a bare name is being typed.
	size_t start = character;
	while (start > 0 && (line[start - 1] == '_'
		|| (line[start - 1] >= 'a' && line[start - 1] <= 'z')
		|| (line[start - 1] >= 'A' && line[start - 1] <= 'Z')
		|| (line[start - 1] >= '0' && line[start - 1] <= '9'))) {
		--start;
	}
	if (start > 0 && line[start - 1] == '.') return {};
	return {CompletionContext::Unqualified, {}};
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

/// Convenience wrapper: picks the zero-based `line` out of `text` first.
[[nodiscard]] inline std::optional<std::vector<std::string>> qualifier_chain_in(
	const std::string_view text, const size_t line, const size_t character) {
	const auto content = line_at(text, line);
	if (!content) return std::nullopt;
	return qualifier_chain_at(*content, character);
}

/// Convenience wrapper for completion_query_at().
[[nodiscard]] inline CompletionQuery completion_query_in(
	const std::string_view text, const size_t line, const size_t character) {
	const auto content = line_at(text, line);
	if (!content) return {};
	return completion_query_at(*content, character);
}

}
