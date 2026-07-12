//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../../misc/SourceLocation.h"

/**
 * One resolved reference -> its declaration location, harvested during LSP analysis.
 *
 * File paths are stored normalized. Ranges are one-based (as produced by the parser);
 * conversion to zero-based LSP positions happens on lookup and when emitting locations.
 */
struct ReferenceLink {
	std::string ref_file;   ///< normalized path of the file containing the reference
	SourceRange ref_range;  ///< where the reference sits (what the user clicks)
	std::string def_file;   ///< normalized path of the declaration's file
	SourceRange def_range;  ///< the declaration's location (the jump target)
};

/**
 * Position -> declaration index built for one analyzed entry.
 *
 * It is a flat snapshot of copied ranges and paths, so it stays valid after the AST that
 * produced it has been destroyed. Lookups map a zero-based (LSP) request position to the
 * most specific reference covering it.
 */
class ReferenceIndex {
	std::vector<ReferenceLink> m_links;

public:
	void add(std::string ref_file, const SourceRange& ref_range, std::string def_file, const SourceRange& def_range) {
		m_links.push_back({std::move(ref_file), ref_range, std::move(def_file), def_range});
	}

	[[nodiscard]] bool empty() const { return m_links.empty(); }

	/// Resolves a zero-based (LSP) position in `file` to a declaration location, if a
	/// reference covers it. Prefers the narrowest covering reference.
	[[nodiscard]] std::optional<ReferenceLink> resolve(const std::string& file, size_t line, size_t character) const {
		const ReferenceLink* best = nullptr;
		for (const auto& link : m_links) {
			if (link.ref_file != file) continue;
			if (!covers(link.ref_range, line, character)) continue;
			if (!best || is_narrower(link.ref_range, best->ref_range)) best = &link;
		}
		if (!best) return std::nullopt;
		return *best;
	}

private:
	static bool covers(const SourceRange& range, size_t line, size_t character) {
		if (!range.is_valid()) return false;
		const size_t start_line = range.start.get_lsp_line();
		const size_t start_char = range.start.get_lsp_char();
		const size_t end_line = range.end.get_lsp_line();
		const size_t end_char = range.end.get_lsp_char();
		const bool after_start = line > start_line || (line == start_line && character >= start_char);
		const bool before_end = line < end_line || (line == end_line && character < end_char);
		return after_start && before_end;
	}

	static bool is_narrower(const SourceRange& a, const SourceRange& b) {
		const size_t lines_a = a.end.get_lsp_line() - a.start.get_lsp_line();
		const size_t lines_b = b.end.get_lsp_line() - b.start.get_lsp_line();
		if (lines_a != lines_b) return lines_a < lines_b;
		return a.end.get_lsp_char() < b.end.get_lsp_char();
	}
};
