//
// Created by Mathias Vatter on 18.08.26.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

/**
 * Names a character the tokenizer cannot use, for a message that must not print it.
 *
 * The characters that actually turn up here are invisible. A zero width space or a
 * non-breaking space arrives by pasting a line out of a website, a PDF or a chat window, and
 * then sits in the source doing nothing a reader can see. Echoing it back - which is all
 * "Found invalid token: <char>" can do - shows an empty box, a question mark, or nothing at
 * all, and leaves the reader looking at a line that appears perfectly fine.
 *
 * The codepoint and a name are the only way to say which character is there, and for the
 * invisible ones the message has to say that being invisible is the point.
 */
namespace invalid_character {

/// One decoded character: its codepoint, how many bytes it occupies, and whether those bytes
/// were a UTF-8 sequence at all.
struct Decoded {
	char32_t codepoint = 0;
	size_t length = 1;
	bool valid = true;
};

/// The character starting at `position`, decoded from UTF-8.
///
/// A byte that starts no valid sequence is returned as itself, marked invalid: that is what a
/// file saved in another encoding produces, and calling its bytes codepoints would name a
/// character the file does not actually contain.
[[nodiscard]] inline Decoded decode(const std::string_view input, const size_t position) {
	if (position >= input.size()) return {};
	const auto lead = static_cast<unsigned char>(input[position]);
	if (lead < 0x80) return {lead, 1};

	size_t length = 0;
	char32_t codepoint = 0;
	if ((lead & 0xE0) == 0xC0) { length = 2; codepoint = lead & 0x1F; }
	else if ((lead & 0xF0) == 0xE0) { length = 3; codepoint = lead & 0x0F; }
	else if ((lead & 0xF8) == 0xF0) { length = 4; codepoint = lead & 0x07; }
	else return {lead, 1, false};

	if (position + length > input.size()) return {lead, 1, false};
	for (size_t index = 1; index < length; ++index) {
		const auto continuation = static_cast<unsigned char>(input[position + index]);
		if ((continuation & 0xC0) != 0x80) return {lead, 1, false};
		codepoint = (codepoint << 6) | (continuation & 0x3F);
	}
	return {codepoint, length};
}

/// What the character is called, or nothing when it has no name worth giving.
[[nodiscard]] inline std::string name_of(const char32_t codepoint) {
	static const std::unordered_map<char32_t, std::string> names = {
		{0x00A0, "no-break space"},
		{0x2000, "en quad"}, {0x2001, "em quad"}, {0x2002, "en space"}, {0x2003, "em space"},
		{0x2007, "figure space"}, {0x2009, "thin space"}, {0x200A, "hair space"},
		{0x200B, "zero width space"}, {0x200C, "zero width non-joiner"},
		{0x200D, "zero width joiner"}, {0x2028, "line separator"},
		{0x2029, "paragraph separator"}, {0x202F, "narrow no-break space"},
		{0x2060, "word joiner"}, {0xFEFF, "zero width no-break space, a byte order mark"},
		{0x2018, "left single quotation mark"}, {0x2019, "right single quotation mark"},
		{0x201C, "left double quotation mark"}, {0x201D, "right double quotation mark"},
		{0x2013, "en dash"}, {0x2014, "em dash"}, {0x2212, "minus sign"},
		{0x00B4, "acute accent"}, {0x0060, "grave accent"}
	};
	const auto it = names.find(codepoint);
	return it != names.end() ? it->second : "";
}

/// Whether the character leaves no mark on screen, which is what makes it hard to find.
[[nodiscard]] inline bool is_invisible(const char32_t codepoint) {
	return codepoint == 0x00A0 || codepoint == 0x2028 || codepoint == 0x2029
		|| codepoint == 0x2060 || codepoint == 0xFEFF
		|| (codepoint >= 0x2000 && codepoint <= 0x200D)
		|| codepoint == 0x202F;
}

/// The text that replaces the character to make the line compile, or nothing when what was
/// meant cannot be told: a space where the character stands for one, and nothing at all where
/// it stands for nothing. A quotation mark or a dash is left alone - it was written for a
/// reason, and only its author knows which.
[[nodiscard]] inline std::optional<std::string> replacement_for(const char32_t codepoint) {
	if (codepoint == 0x200B || codepoint == 0x200C || codepoint == 0x200D
		|| codepoint == 0x2060 || codepoint == 0xFEFF) {
		return "";
	}
	if (codepoint == 0x00A0 || codepoint == 0x202F
		|| (codepoint >= 0x2000 && codepoint <= 0x200A)) {
		return " ";
	}
	return std::nullopt;
}

/// "U+200B (zero width space)", the part of the message that says which character it is.
[[nodiscard]] inline std::string describe(const char32_t codepoint) {
	char buffer[16];
	std::snprintf(buffer, sizeof(buffer), "U+%04X", static_cast<unsigned>(codepoint));
	const auto name = name_of(codepoint);
	return name.empty() ? std::string(buffer) : std::string(buffer) + " (" + name + ")";
}

} // namespace invalid_character
