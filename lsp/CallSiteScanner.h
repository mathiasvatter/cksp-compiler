//
// Created for LSP signature-help support.
//

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "SourceTextScanner.h"

namespace lsp {

struct CallSiteQuery {
	/// Everything left of the callable's final name: `audio.inst` in `audio.inst.fade(`.
	std::vector<std::string> qualifier;
	/// The final source-level name: `fade` in `audio.inst.fade(`.
	std::string callable;
	/// Zero-based argument containing the cursor.
	size_t active_parameter = 0;
};

namespace detail {

struct OpenDelimiter {
	char delimiter = 0;
	std::optional<std::vector<std::string>> callable_chain;
	size_t active_parameter = 0;
};

}

/**
 * Finds the innermost active call at a position in the live editor buffer.
 *
 * The buffer is scanned forwards because only that direction can reliably distinguish
 * delimiters inside strings and comments. Each open parenthesis remembers the access chain
 * immediately before it; grouping parentheses therefore remain on the delimiter stack but
 * are not mistaken for calls. Commas only advance the parenthesis they occur directly in,
 * so nested calls and array literals do not change the outer call's active parameter.
 */
[[nodiscard]] inline std::optional<CallSiteQuery> call_site_at(
	const std::string_view text, const size_t cursor) {
	if (cursor > text.size()) return std::nullopt;

	std::vector<detail::OpenDelimiter> delimiters;
	source_text::LexicalState lexical_state;

	for (size_t i = 0; i < cursor; ++i) {
		if (source_text::consume_non_code(text, i, lexical_state)) continue;
		const char c = text[i];

		if (c == cksp::lexical::OPEN_PARENTHESIS) {
			delimiters.push_back({c, source_text::access_chain_ending_at(text, i), 0});
		} else if (c == cksp::lexical::OPEN_BRACKET) {
			delimiters.push_back({c, std::nullopt, 0});
		} else if (c == cksp::lexical::CLOSE_PARENTHESIS
			|| c == cksp::lexical::CLOSE_BRACKET) {
			if (!delimiters.empty()
				&& cksp::lexical::delimiters_match(delimiters.back().delimiter, c)) {
				delimiters.pop_back();
			}
		} else if (c == ',' && !delimiters.empty()
			&& delimiters.back().delimiter == cksp::lexical::OPEN_PARENTHESIS) {
			++delimiters.back().active_parameter;
		}
	}

	// Signature help inside a comment is noise. Inside a string it remains useful and all
	// delimiters in the literal were ignored above, so the surrounding call is still exact.
	if (lexical_state.in_comment()) return std::nullopt;
	for (auto it = delimiters.rbegin(); it != delimiters.rend(); ++it) {
		if (it->delimiter != cksp::lexical::OPEN_PARENTHESIS
			|| !it->callable_chain || it->callable_chain->empty()) continue;
		auto chain = *it->callable_chain;
		auto callable = std::move(chain.back());
		chain.pop_back();
		return CallSiteQuery{
			.qualifier = std::move(chain),
			.callable = std::move(callable),
			.active_parameter = it->active_parameter,
		};
	}
	return std::nullopt;
}

/// Convenience wrapper for a zero-based LSP position.
[[nodiscard]] inline std::optional<CallSiteQuery> call_site_in(
	const std::string_view text, const size_t line, const size_t character) {
	const auto cursor = source_text::offset_at(text, line, character);
	return cursor ? call_site_at(text, *cursor) : std::nullopt;
}

}
