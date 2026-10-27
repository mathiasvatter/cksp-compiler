#pragma once

#include <optional>
#include <string_view>

#include "LineIndex.h"
#include "TextChange.h"
#include "../misc/SourceLocation.h"

namespace lsp {

/// Carries one-based source positions from an old version of a text onto a new one, by
/// the rules of TextChange::map(): a position whose text was edited has no counterpart.
class PositionMap {
	TextChange m_change;
	LineIndex m_old_lines;
	LineIndex m_new_lines;

public:
	PositionMap(const std::string_view before, const std::string_view after)
		: m_change(before, after), m_old_lines(before), m_new_lines(after) {}

	[[nodiscard]] std::optional<SourcePosition> map(
		const SourcePosition& position, const TextChange::Bias bias) const {
		const auto old_offset = m_old_lines.offset_at(position.get_lsp_line(), position.get_lsp_char());
		const auto new_offset = old_offset ? m_change.map(*old_offset, bias) : std::nullopt;
		if (!new_offset) return std::nullopt;
		const auto [line, character] = m_new_lines.line_and_character(*new_offset);
		return SourcePosition{line + 1, character + 1};
	}

	/// A range keeps its place only when both ends do and it stays on one line.
	[[nodiscard]] std::optional<SourceRange> map(const SourceRange& range) const {
		const auto start = map(range.start, TextChange::Bias::Following);
		const auto end = start ? map(range.end, TextChange::Bias::Preceding) : std::nullopt;
		if (!end || start->line != end->line) return std::nullopt;
		return SourceRange{*start, *end};
	}
};

}
