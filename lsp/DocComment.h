//
// Doc comments written at a declaration, for hovers.
//

#pragma once

#include <regex>
#include <string>
#include <string_view>
#include <vector>

/*
 * Which comments count as documentation, and how they are shown.
 *
 * The comment that ends on the line right above a declaration, without a blank line between,
 * documents it - in any spelling the common docstring conventions use:
 *
 *   line comments     //  ///  //!          (Doxygen, plain)
 *   block comments    / *  / **  / *!  ... * /   (JSDoc, TSDoc, Doxygen, plain)
 *   KSP comments      { ... }  {doc: ... }  (SublimeKSP and the cksp-tools convention)
 *
 * A comment trailing the declaration on its own line documents it as well when it is a
 * Doxygen member comment (//!<, ///<, / *!< , / **<).
 *
 * Block tags are recognised in both JSDoc and Doxygen spelling (@param, \param) and shown the
 * way TypeScript's quick info shows them: "*@param* `name` — text". @brief only marks the
 * summary and is dropped.
 */
namespace lsp {

namespace doc_comment_detail {

inline std::string_view trim(std::string_view text) {
	const auto first = text.find_first_not_of(" \t\r");
	if (first == std::string_view::npos) return {};
	const auto last = text.find_last_not_of(" \t\r");
	return text.substr(first, last - first + 1);
}

inline bool starts_with(const std::string_view text, const std::string_view prefix) {
	return text.substr(0, prefix.size()) == prefix;
}

inline bool ends_with(const std::string_view text, const std::string_view suffix) {
	return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

inline std::vector<std::string_view> split_lines(const std::string_view text) {
	std::vector<std::string_view> lines;
	for (size_t start = 0; start <= text.size();) {
		const auto end = text.find('\n', start);
		lines.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
		if (end == std::string_view::npos) break;
		start = end + 1;
	}
	return lines;
}

/// One comment line without its markers - openers, closers, the <*> gutter of a block
/// comment - and without lines of pure decoration (<-----> or <*****>).
inline std::string strip_markers(std::string_view line) {
	line = trim(line);
	for (const std::string_view opener : {"{doc:", "/**", "/*!", "/*", "{", "///", "//!", "//"}) {
		if (starts_with(line, opener)) {
			line.remove_prefix(opener.size());
			break;
		}
	}
	// Doxygen member comments point back at the declaration with a <.
	if (starts_with(line, "<")) line.remove_prefix(1);
	if (ends_with(line, "*/")) line.remove_suffix(2);
	else if (ends_with(line, "}")) line.remove_suffix(1);
	line = trim(line);
	if (starts_with(line, "*")) line = trim(line.substr(1));
	if (line.find_first_not_of("*-#=/_~ ") == std::string_view::npos) return {};
	return std::string(line);
}

/// "*@param* `name` — text" for a block tag line, the line itself for prose.
inline std::string format_tags(std::string line) {
	static const std::regex tag(R"(^[@\\](\w+)\s*(.*)$)");
	std::smatch match;
	if (!std::regex_match(line, match, tag)) return line;
	auto name = match[1].str();
	auto rest = match[2].str();
	if (name == "brief" || name == "short" || name == "summary") return rest;
	if (name == "return") name = "returns";
	if (name == "arg" || name == "argument") name = "param";

	if (name == "param" || name == "tparam" || name == "property" || name == "throws" || name == "exception") {
		// JSDoc writes the type first ({int} amount), Doxygen a direction ([in] amount).
		static const std::regex parameter(R"(^(\[[^\]]*\]\s*)?(\{[^}]*\}\s*)?([\w#.$\[\]=]+)\s*[-:]?\s*(.*)$)");
		std::smatch parts;
		if (std::regex_match(rest, parts, parameter)) {
			auto text = "*@" + name + "* `" + parts[3].str() + "`";
			if (parts[2].matched) text += " `" + std::string(trim(parts[2].str())) + "`";
			if (parts[4].length() > 0) text += " — " + parts[4].str();
			return text;
		}
	}
	return "*@" + name + "*" + (rest.empty() ? "" : " — " + rest);
}

inline std::string to_markdown(const std::vector<std::string_view>& comment_lines) {
	std::vector<std::string> lines;
	for (const auto& line : comment_lines) {
		auto stripped = strip_markers(line);
		if (!stripped.empty()) lines.push_back(format_tags(std::move(stripped)));
	}
	// Prose runs on as written; each tag starts a paragraph of its own, as TypeScript sets it.
	std::string markdown;
	bool previous_was_tag = false;
	for (const auto& line : lines) {
		const bool is_tag = starts_with(line, "*@");
		if (!markdown.empty()) markdown += (is_tag || previous_was_tag) ? "\n\n" : "  \n";
		markdown += line;
		previous_was_tag = is_tag;
	}
	return markdown;
}

/// The Doxygen member comment trailing a declaration on its own line, if there is one.
inline std::string_view trailing_member_comment(const std::string_view line) {
	for (const std::string_view marker : {"//!<", "///<", "/*!<", "/**<"}) {
		if (const auto at = line.find(marker); at != std::string_view::npos) return line.substr(at);
	}
	return {};
}

}  // namespace doc_comment_detail

/// What the doc comment of a function or macro says about one of its parameters: the text
/// of its @param, or empty. A macro parameter <#name#> may be documented as <name>.
inline std::string parameter_doc(const std::string& documentation, std::string parameter) {
	if (parameter.size() > 2 && parameter.front() == '#' && parameter.back() == '#') {
		parameter = parameter.substr(1, parameter.size() - 2);
	}
	for (size_t start = 0; start < documentation.size();) {
		auto end = documentation.find("\n\n", start);
		if (end == std::string::npos) end = documentation.size();
		const auto paragraph = std::string_view(documentation).substr(start, end - start);
		for (const auto& spelling : {parameter, "#" + parameter + "#"}) {
			const auto head = "*@param* `" + spelling + "`";
			if (!doc_comment_detail::starts_with(paragraph, head)) continue;
			const auto dash = paragraph.find(" — ");
			return dash == std::string_view::npos ? std::string{} : std::string(paragraph.substr(dash + 5));
		}
		start = end + 2;
	}
	return {};
}

/// The documentation of the declaration on a zero-based line, as markdown, or empty.
inline std::string doc_comment_above(const std::string_view text, const size_t line) {
	using namespace doc_comment_detail;
	const auto lines = split_lines(text);
	if (line >= lines.size()) return {};

	// Collected bottom-up, so in reverse.
	std::vector<std::string_view> comment;
	std::string_view block_opener;  // the opener of the block comment being walked through
	for (size_t i = line; i-- > 0;) {
		const auto current = trim(lines[i]);
		if (!block_opener.empty()) {
			comment.push_back(current);
			if (starts_with(current, block_opener)) block_opener = {};
			continue;
		}
		if (current.empty()) break;
		if (starts_with(current, "//")) {
			comment.push_back(current);
		} else if (ends_with(current, "*/") || ends_with(current, "}")) {
			const std::string_view opener = ends_with(current, "*/") ? "/*" : "{";
			comment.push_back(current);
			if (!starts_with(current, opener)) block_opener = opener;
		} else {
			break;
		}
	}
	// A block whose opener never came was code ending in a brace, not a comment.
	if (!block_opener.empty()) comment.clear();

	std::vector<std::string_view> ordered(comment.rbegin(), comment.rend());
	if (const auto trailing = trailing_member_comment(lines[line]); !trailing.empty()) {
		ordered.push_back(trailing);
	}
	return to_markdown(ordered);
}

}  // namespace lsp
