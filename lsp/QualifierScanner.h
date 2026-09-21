//
// Created for LSP completion support.
//

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SourceTextScanner.h"

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

	if (source_text::lexical_state_at(line, character).outside_code()) return std::nullopt;

	// Skip the partial identifier being typed, so completion keeps working when the
	// request arrives mid-word (`audio.ra|`) and not only right behind the dot.
	size_t dot = character;
	while (dot > 0 && source_text::is_identifier_char(line[dot - 1])) --dot;
	if (dot == 0 || line[dot - 1] != '.') return std::nullopt;

	return source_text::access_chain_ending_at(line, dot - 1);
}

/// True when the position sits inside a string literal or a comment on its line.
[[nodiscard]] inline bool in_string_or_comment(const std::string_view line, const size_t character) {
	return source_text::lexical_state_at(line, character).outside_code();
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
	while (start > 0 && source_text::is_identifier_char(line[start - 1])) {
		--start;
	}
	if (start > 0 && line[start - 1] == '.') return {};
	return {CompletionContext::Unqualified, {}};
}

/// The zero-based `line` of `text`, without its line ending.
using source_text::line_at;

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
