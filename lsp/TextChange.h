#pragma once

#include <algorithm>
#include <optional>
#include <string_view>

namespace lsp {

/**
 * The span in which two versions of a text differ, taken as everything between their
 * common prefix and common suffix.
 *
 * While typing, the edits between two analyses usually are one such span, so it carries
 * positions computed on the old version over to the new one without a full diff.
 */
class TextChange {
	size_t m_prefix = 0;
	size_t m_old_end = 0;
	size_t m_new_end = 0;

public:
	TextChange(const std::string_view before, const std::string_view after) {
		const auto shorter = std::min(before.size(), after.size());
		while (m_prefix < shorter && before[m_prefix] == after[m_prefix]) ++m_prefix;
		size_t suffix = 0;
		while (suffix < shorter - m_prefix
			&& before[before.size() - 1 - suffix] == after[after.size() - 1 - suffix]) {
			++suffix;
		}
		m_old_end = before.size() - suffix;
		m_new_end = after.size() - suffix;
	}

	/// Which text a position belongs to, which decides it where an insertion lands right
	/// on it: the start of a name moves along with the name, while its end stays only as
	/// long as nothing is typed onto the name.
	enum class Bias {
		Preceding,  ///< the text before the position, like the end of a name
		Following,  ///< the text after the position, like the start of a name
	};

	/// Where an offset of the old text lies in the new one. Offsets before the span keep
	/// their place and those after it shift with the text; one whose text the span touches
	/// has no counterpart.
	[[nodiscard]] std::optional<size_t> map(const size_t offset, const Bias bias) const {
		if (offset < m_prefix) return offset;
		const bool after_span = bias == Bias::Following ? offset >= m_old_end : offset > m_old_end;
		if (after_span) return offset - m_old_end + m_new_end;
		return std::nullopt;
	}
};

}
