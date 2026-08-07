//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
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
	SourceRange def_range;  ///< the declaration's location (the jump target; spans the whole header for functions)
	SourceRange def_name_range;  ///< exactly the declared name, e.g. the range a rename edit replaces
	/// Whether the source text at `ref_range` is the declared name itself. A macro body
	/// reaches a declaration through a parameter (<#browser#> for <browser>), so the text
	/// the user sees there is the parameter, not the name: the reference is real and worth
	/// navigating and listing, but a rename must not rewrite it.
	bool spelled_as_declared = true;
};

/**
 * One definition-only navigation link.
 *
 * Unlike ReferenceLink, this is deliberately not a symbol relationship: references,
 * rename and document highlights must ignore it. Import path -> imported file is the
 * primary use case.
 */
struct DefinitionLink {
	std::string ref_file;
	SourceRange ref_range;
	std::string def_file;
	SourceRange def_range;
	SourceRange def_selection_range;
	std::string tooltip;
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
	std::vector<DefinitionLink> m_definition_links;
	std::unordered_set<std::string> m_seen_links;
	std::unordered_set<std::string> m_seen_definition_links;
	std::unordered_set<std::string> m_seen_references;
	mutable std::unordered_map<std::string, std::string> m_normalized_files;

public:
	/// Records a reference -> declaration link. The same visible source range can legitimately
	/// carry both a preprocessor link and an AST semantic link after macro expansion, so dedupe
	/// keeps distinct declaration targets while still suppressing exact duplicates.
	void add(std::string ref_file, const SourceRange& ref_range, std::string def_file, const SourceRange& def_range) {
		add(std::move(ref_file), ref_range, std::move(def_file), def_range, def_range);
	}

	/// Same as add(), with a separate name range when the declaration range spans more than
	/// the declared name (function headers span name, parameters and parenthesis).
	void add(std::string ref_file, const SourceRange& ref_range, std::string def_file, const SourceRange& def_range, const SourceRange& def_name_range, const bool spelled_as_declared = true) {
		ref_file = normalized_file(ref_file);
		def_file = normalized_file(def_file);
		const auto ref_key = reference_key(ref_file, ref_range);
		const auto link_key = ref_key + "=>" + reference_key(def_file, def_range);
		if (!m_seen_links.insert(link_key).second) return;
		m_seen_references.insert(ref_key);
		m_links.push_back({
			std::move(ref_file), ref_range, std::move(def_file), def_range, def_name_range,
			spelled_as_declared});
	}

	/// Records a link between two source tokens (reference -> declaration). Tokens without a
	/// real source file (builtins, synthesized nodes) are skipped. Used by preprocessing and
	/// by AST prefix provenance, where the declaration token is the symbol identity.
	void add_link(const Token& reference, const Token& declaration) {
		if (reference.file.empty() || declaration.file.empty()) return;
		const auto ref_range = source_range_from_token(reference);
		const auto def_range = source_range_from_token(declaration);
		if (!ref_range.is_valid() || !def_range.is_valid()) return;
		add(reference.file, ref_range, declaration.file, def_range);
	}

	/// Records a path token -> file link for go-to-definition and document links.
	/// String quotes are excluded from the clickable source range.
	void add_file_link(const Token& path_token, std::string target_file, std::string tooltip = {}) {
		if (path_token.file.empty()) return;
		auto path_range = source_range_from_token(path_token);
		if (path_token.type == token::STRING && path_token.val.size() >= 2) {
			++path_range.start.column;
			--path_range.end.column;
		}
		add_definition_link(
			path_token.file,
			path_range,
			std::move(target_file),
			SourceRange{{0, 0}, {0, 0}},
			std::move(tooltip)
		);
	}

	/// Records a one-way go-to-definition link that is invisible to symbol operations.
	void add_definition_link(
		std::string ref_file,
		const SourceRange& ref_range,
		std::string def_file,
		const SourceRange& def_range,
		std::string tooltip = {}) {
		if (!ref_range.is_valid() || !def_range.is_valid()) return;
		ref_file = normalized_file(ref_file);
		def_file = normalized_file(def_file);
		const auto link_key = reference_key(ref_file, ref_range)
			+ "=>" + reference_key(def_file, def_range);
		if (!m_seen_definition_links.insert(link_key).second) return;
		m_definition_links.push_back({
			std::move(ref_file), ref_range, std::move(def_file), def_range, def_range,
			std::move(tooltip)
		});
	}

	[[nodiscard]] bool empty() const { return m_links.empty() && m_definition_links.empty(); }

	/// Resolves definition-only navigation first, then falls back to normal symbols.
	/// A position may lead to several declarations; see resolve_all().
	[[nodiscard]] std::vector<DefinitionLink> resolve_definition(
		const std::string& file,
		const size_t line,
		const size_t character) const {
		const DefinitionLink* best = nullptr;
		for (const auto& link : m_definition_links) {
			if (link.ref_file != file) continue;
			if (!covers(link.ref_range, line, character)) continue;
			if (!best || is_narrower(link.ref_range, best->ref_range)) best = &link;
		}
		if (best) return {*best};

		std::vector<DefinitionLink> found;
		for (const auto& symbol : resolve_all(file, line, character)) {
			found.push_back(DefinitionLink{
				symbol.ref_file,
				symbol.ref_range,
				symbol.def_file,
				symbol.def_range,
				symbol.def_name_range,
				{}
			});
		}
		// The position may sit on a declaration rather than on a usage.
		if (found.empty()) {
			if (auto symbol = resolve_target(file, line, character)) {
				found.push_back(DefinitionLink{
					symbol->ref_file,
					symbol->ref_range,
					symbol->def_file,
					symbol->def_range,
					symbol->def_name_range,
					{}
				});
			}
		}
		return found;
	}

	/// Returns definition-only links originating in one document.
	[[nodiscard]] std::vector<DefinitionLink> definition_links_in(const std::string& file) const {
		std::vector<DefinitionLink> links;
		for (const auto& link : m_definition_links) {
			if (link.ref_file == file) links.push_back(link);
		}
		return links;
	}

	/// True when current analysis already owns a definition-only link at this source range.
	[[nodiscard]] bool contains_definition_link(
		const std::string& file,
		const SourceRange& range) const {
		for (const auto& link : m_definition_links) {
			if (link.ref_file == file && same_range(link.ref_range, range)) return true;
		}
		return false;
	}

	/// Resolves a zero-based (LSP) position in `file` to a declaration location, if a
	/// reference covers it. Prefers the narrowest covering reference.
	[[nodiscard]] std::optional<ReferenceLink> resolve(const std::string& file, size_t line, size_t character) const {
		auto found = resolve_all(file, line, character);
		if (found.empty()) return std::nullopt;
		return std::move(found.front());
	}

	/**
	 * Every declaration the narrowest reference covering a position leads to.
	 *
	 * Usually one, but a macro parameter genuinely has two: the parameter in the macro
	 * header, recorded while substituting, and whatever the argument passed at the call
	 * site resolves to. Both are written at the same place in the body, so both are the
	 * honest answer to "where is this defined" - the editor lets the user pick.
	 *
	 * Ordered as recorded, so the preprocessor's parameter link leads.
	 */
	[[nodiscard]] std::vector<ReferenceLink> resolve_all(const std::string& file, size_t line, size_t character) const {
		const SourceRange* narrowest = nullptr;
		for (const auto& link : m_links) {
			if (link.ref_file != file) continue;
			if (!covers(link.ref_range, line, character)) continue;
			if (!narrowest || is_narrower(link.ref_range, *narrowest)) narrowest = &link.ref_range;
		}
		if (!narrowest) return {};

		std::vector<ReferenceLink> found;
		for (const auto& link : m_links) {
			if (link.ref_file != file || !same_range(link.ref_range, *narrowest)) continue;
			found.push_back(link);
		}
		return found;
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

	/// True when the zero-based (LSP) position lies inside the one-based range.
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

private:
	[[nodiscard]] std::string normalized_file(const std::string& file) const {
		if (const auto found = m_normalized_files.find(file); found != m_normalized_files.end()) {
			return found->second;
		}
		auto normalized = FileSystemSourceProvider::normalize(file).value;
		m_normalized_files.emplace(file, normalized);
		return normalized;
	}

	static std::string reference_key(const std::string& file, const SourceRange& range) {
		return file + "@"
			+ std::to_string(range.start.line) + ":" + std::to_string(range.start.column) + "-"
			+ std::to_string(range.end.line) + ":" + std::to_string(range.end.column);
	}

	static bool same_range(const SourceRange& a, const SourceRange& b) {
		return a.start.line == b.start.line && a.start.column == b.start.column
			&& a.end.line == b.end.line && a.end.column == b.end.column;
	}

	static bool is_narrower(const SourceRange& a, const SourceRange& b) {
		const size_t lines_a = a.end.get_lsp_line() - a.start.get_lsp_line();
		const size_t lines_b = b.end.get_lsp_line() - b.start.get_lsp_line();
		if (lines_a != lines_b) return lines_a < lines_b;
		return a.end.get_lsp_char() < b.end.get_lsp_char();
	}
};
