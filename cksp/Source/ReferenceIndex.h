//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "../../misc/SourceLocation.h"
#include "../Tokenizer/Token.h"
#include "SourceProvider.h"

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

/** A named source construct which can be used as a qualifier, such as a namespace. */
struct QualifierDefinition {
	std::string name;
	std::string file;
	SourceRange range;
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
	std::unordered_set<std::string> m_seen_references;
	std::vector<QualifierDefinition> m_qualifier_definitions;
	std::unordered_set<std::string> m_seen_qualifier_definitions;

public:
	/// Records a reference -> declaration link. A reference location is indexed only once:
	/// if a link for the same file and reference range already exists it is kept, so an earlier
	/// (e.g. pre-lowering) pass wins over a later one for the same reference.
	void add(std::string ref_file, const SourceRange& ref_range, std::string def_file, const SourceRange& def_range) {
		if (!m_seen_references.insert(reference_key(ref_file, ref_range)).second) return;
		m_links.push_back({std::move(ref_file), ref_range, std::move(def_file), def_range});
	}

	/// Records a link between two source tokens (reference -> declaration). Tokens without a
	/// real source file (builtins, synthesized nodes) are skipped. Used by the preprocessor
	/// passes for define and macro usages.
	void add_link(const Token& reference, const Token& declaration) {
		if (reference.file.empty() || declaration.file.empty()) return;
		const auto ref_range = source_range_from_token(reference);
		const auto def_range = source_range_from_token(declaration);
		if (!ref_range.is_valid() || !def_range.is_valid()) return;
		add(FileSystemSourceProvider::normalize(reference.file).value, ref_range,
			FileSystemSourceProvider::normalize(declaration.file).value, def_range);
	}

	/// Records a named qualifier definition before desugaring removes its AST node.
	void add_qualifier_definition(std::string name, const Token& declaration) {
		if (declaration.file.empty()) return;
		const auto range = source_range_from_token(declaration);
		if (!range.is_valid()) return;
		auto file = FileSystemSourceProvider::normalize(declaration.file).value;
		auto key = name + "@" + file + "@" + range.to_string();
		if (!m_seen_qualifier_definitions.insert(std::move(key)).second) return;
		m_qualifier_definitions.push_back({std::move(name), std::move(file), range});
	}

	/// Finds the qualifier in the file which owns the referenced member. Requiring the same
	/// file prevents equal namespace/family names in separate imports from being conflated.
	[[nodiscard]] std::optional<QualifierDefinition> qualifier_definition(
		const std::string& name,
		const std::string& preferred_file) const {
		const auto normalized_preferred = FileSystemSourceProvider::normalize(preferred_file).value;
		for (const auto& definition : m_qualifier_definitions) {
			if (definition.name == name && definition.file == normalized_preferred) return definition;
		}
		return std::nullopt;
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

	/// Resolves the declaration represented at a position. The position may be either a
	/// reference or the declaration itself. References take precedence when ranges overlap.
	[[nodiscard]] std::optional<ReferenceLink> resolve_target(const std::string& file, size_t line, size_t character) const {
		if (auto reference = resolve(file, line, character)) return reference;

		const ReferenceLink* best = nullptr;
		for (const auto& link : m_links) {
			if (link.def_file != file) continue;
			if (!covers(link.def_range, line, character)) continue;
			if (!best || is_narrower(link.def_range, best->def_range)) best = &link;
		}
		if (!best) return std::nullopt;
		return *best;
	}

	/// Returns every indexed usage that resolves to the same declaration as target.
	[[nodiscard]] std::vector<ReferenceLink> references_to(const ReferenceLink& target) const {
		std::vector<ReferenceLink> references;
		for (const auto& link : m_links) {
			if (link.def_file == target.def_file && same_range(link.def_range, target.def_range)) {
				references.push_back(link);
			}
		}
		return references;
	}

	/// True when this snapshot already has a (possibly differently resolved) link at range.
	/// Used when layering a partial current index over the last successful snapshot.
	[[nodiscard]] bool contains_reference(const std::string& file, const SourceRange& range) const {
		return m_seen_references.contains(reference_key(file, range));
	}

private:
	static std::string reference_key(const std::string& file, const SourceRange& range) {
		return file + "@"
			+ std::to_string(range.start.line) + ":" + std::to_string(range.start.column) + "-"
			+ std::to_string(range.end.line) + ":" + std::to_string(range.end.column);
	}

	static bool same_range(const SourceRange& a, const SourceRange& b) {
		return a.start.line == b.start.line && a.start.column == b.start.column
			&& a.end.line == b.end.line && a.end.column == b.end.column;
	}

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
