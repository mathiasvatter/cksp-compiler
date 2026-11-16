#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "../JSON/ast/JSONValue.h"

namespace lsp {

/// Converts between byte offsets and zero-based LSP line/character pairs of one text.
class LineIndex {
	std::vector<size_t> m_starts{0};
	size_t m_size = 0;

public:
	explicit LineIndex(const std::string_view text) : m_size(text.size()) {
		for (size_t i = 0; i < text.size(); ++i) {
			if (text[i] == '\n') m_starts.push_back(i + 1);
		}
	}

	[[nodiscard]] std::pair<size_t, size_t> line_and_character(const size_t offset) const {
		const auto next = std::ranges::upper_bound(m_starts, offset);
		const auto line = static_cast<size_t>(next - m_starts.begin()) - 1;
		return {line, offset - m_starts[line]};
	}

	/// The offset of a position, or nothing when the text has no such line or column.
	[[nodiscard]] std::optional<size_t> offset_at(const size_t line, const size_t character) const {
		if (line >= m_starts.size()) return std::nullopt;
		const auto line_end = line + 1 < m_starts.size() ? m_starts[line + 1] - 1 : m_size;
		if (character > line_end - m_starts[line]) return std::nullopt;
		return m_starts[line] + character;
	}

	[[nodiscard]] std::unique_ptr<JSONObject> position_of(const size_t offset) const {
		const auto [line, character] = line_and_character(offset);
		auto position = std::make_unique<JSONObject>();
		position->add("line", std::make_unique<JSONInt>(static_cast<long long>(line)));
		position->add("character", std::make_unique<JSONInt>(static_cast<long long>(character)));
		return position;
	}
};

}
