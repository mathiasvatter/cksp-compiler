//
// Created for LSP signature-help support.
//

#pragma once

#include <algorithm>
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

/// One argument of a complete call, without its surrounding whitespace.
struct CallArgument {
	size_t offset = 0;
	std::string_view text;
};

/// A complete call written in the buffer, with every argument it passes.
struct CallSite {
	std::vector<std::string> qualifier;
	std::string callable;
	/// Byte offset of the callable's final name.
	size_t name_offset = 0;
	std::vector<CallArgument> arguments;
};

namespace detail {

/// Keywords whose following name declares a callable rather than calling one.
[[nodiscard]] inline bool is_declaration_keyword(const std::string_view word) {
	return word == "function" || word == "taskfunc" || word == "macro"
		|| word == "define" || word == "on";
}

/// True when the access chain whose final name starts at `name_offset` follows a
/// declaration keyword: <function audio.fade(> declares, it does not call.
[[nodiscard]] inline bool follows_declaration_keyword(
	const std::string_view text, size_t position) {
	const auto skip_space = [&] {
		while (position > 0 && source_text::is_horizontal_space(text[position - 1])) --position;
	};
	const auto skip_name = [&] {
		while (position > 0 && source_text::is_identifier_char(text[position - 1])) --position;
	};
	while (true) {
		skip_space();
		if (position == 0 || text[position - 1] != '.') break;
		--position;
		skip_space();
		skip_name();
	}
	const size_t word_end = position;
	skip_name();
	return is_declaration_keyword(text.substr(position, word_end - position));
}

[[nodiscard]] inline bool starts_non_code(const std::string_view text, const size_t position) {
	const char c = text[position];
	if (c == '{') return true;
	if (position + 1 >= text.size()) return false;
	const char next = text[position + 1];
	return (c == '/' && (next == '/' || next == '*')) || (c == '(' && next == '*');
}

struct OpenCall {
	char delimiter = 0;
	std::optional<CallSite> call;
	/// Start of the argument being read, unset until its first code character.
	std::optional<size_t> argument_start;
	/// End of the argument's last code character so far.
	size_t argument_end = 0;
};

}

/**
 * Finds every complete call whose name starts inside [begin, end).
 *
 * Uses the same forward scan as call_site_at, so strings, comments, grouping parentheses
 * and array literals are told apart the same way. The scan always starts at the buffer
 * start because the lexical state at `begin` depends on everything before it.
 */
[[nodiscard]] inline std::vector<CallSite> call_sites_in(
	const std::string_view text, const size_t begin, const size_t end) {
	std::vector<CallSite> calls;
	std::vector<detail::OpenCall> open;
	source_text::LexicalState lexical_state;

	const auto close_argument = [](detail::OpenCall& frame, const std::string_view source) {
		if (!frame.call) return;
		if (frame.argument_start) {
			frame.call->arguments.push_back({
				*frame.argument_start,
				source.substr(*frame.argument_start, frame.argument_end - *frame.argument_start),
			});
		} else {
			// Keep positions aligned with the parameters even for an empty argument.
			frame.call->arguments.push_back({});
		}
		frame.argument_start.reset();
	};

	for (size_t i = 0; i < text.size(); ++i) {
		const bool was_comment = lexical_state.in_comment();
		const size_t start = i;
		const bool begins_comment = !lexical_state.outside_code() && detail::starts_non_code(text, i);
		if (source_text::consume_non_code(text, i, lexical_state)) {
			// A string literal is argument text; a comment, including its closing
			// delimiter, is not.
			if (!open.empty() && !was_comment && !begins_comment && !lexical_state.in_comment()) {
				auto& frame = open.back();
				if (!frame.argument_start) frame.argument_start = start;
				frame.argument_end = i + 1;
			}
			continue;
		}
		const char c = text[i];

		if (c == cksp::lexical::OPEN_PARENTHESIS) {
			detail::OpenCall frame{c};
			if (auto chain = source_text::access_chain_ending_at(text, i);
				chain && !chain->empty()) {
				size_t name_end = i;
				while (name_end > 0 && source_text::is_horizontal_space(text[name_end - 1])) --name_end;
				const auto& name = chain->back();
				if (name_end >= name.size()
					&& text.substr(name_end - name.size(), name.size()) == name
					&& !detail::follows_declaration_keyword(text, name_end - name.size())) {
					CallSite call;
					call.callable = std::move(chain->back());
					chain->pop_back();
					call.qualifier = std::move(*chain);
					call.name_offset = name_end - call.callable.size();
					frame.call = std::move(call);
				}
			}
			if (!open.empty() && !open.back().argument_start) open.back().argument_start = i;
			open.push_back(std::move(frame));
			continue;
		}
		if (c == cksp::lexical::OPEN_BRACKET) {
			if (!open.empty() && !open.back().argument_start) open.back().argument_start = i;
			open.push_back({c});
			continue;
		}
		if (c == cksp::lexical::CLOSE_PARENTHESIS || c == cksp::lexical::CLOSE_BRACKET) {
			if (open.empty() || !cksp::lexical::delimiters_match(open.back().delimiter, c)) continue;
			auto frame = std::move(open.back());
			open.pop_back();
			if (frame.call && (frame.argument_start || !frame.call->arguments.empty())) {
				close_argument(frame, text);
			}
			if (frame.call && frame.call->name_offset >= begin && frame.call->name_offset < end) {
				calls.push_back(std::move(*frame.call));
			}
			if (!open.empty()) open.back().argument_end = i + 1;
			continue;
		}
		if (open.empty()) continue;
		auto& frame = open.back();
		if (c == ',' && frame.delimiter == cksp::lexical::OPEN_PARENTHESIS) {
			close_argument(frame, text);
			continue;
		}
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
		if (!frame.argument_start) frame.argument_start = i;
		frame.argument_end = i + 1;
	}

	std::ranges::sort(calls, {}, &CallSite::name_offset);
	return calls;
}

}
