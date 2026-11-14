//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <algorithm>
#include <functional>
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

/// The inferred type of a declaration written without one, shown after its declarator.
struct TypeHint {
	std::string file;
	SourcePosition position;  ///< one-based, right after the name (or the array brackets)
	std::string type;
};

/// What hovering a name shows: the declaration it stands for, rendered like a signature.
struct Hover {
	std::string file;
	SourceRange range;  ///< the hovered name, one-based
	std::string text;   ///< "declare mode: int", "function fade(amount: int): int", ...
	std::string declaration_file;
	SourceRange declaration_range;  ///< the declared name, where a doc comment sits above
	std::string documentation;      ///< markdown of that comment; filled in on request
	/// Set for a parameter: its doc is the matching @param of the comment above the header.
	std::string parameter;
};

/// What a name in the source stands for, as far as an editor colours it.
enum class SymbolKind {
	Namespace,
	Struct,
	Enum,  ///< a const block
	Function,
	Method,
	Parameter,
	Variable,
	Property,  ///< a struct member
	Define,
	Macro,
	/// An operator written on a struct, which runs the struct's overload of it.
	Operator,
};

/// One occurrence of a symbol in the source, for semantic highlighting.
struct SymbolToken {
	/// What else holds for this occurrence; bits of `modifiers`.
	enum Modifier : unsigned {
		Declaration = 1u << 0,
		Readonly = 1u << 1,
		Static = 1u << 2,
		/// Defined in the script rather than by the language; marks an Operator.
		UserDefined = 1u << 3,
		/// Provided by the language: a KSP command or engine variable, or a CKSP builtin.
		DefaultLibrary = 1u << 4,
		/// A parameter written <#name#>, which is replaced as text, inside other words too.
		Substitution = 1u << 5,
	};

	std::string file;
	SourceRange range;  ///< one-based, on a single line
	SymbolKind kind;
	unsigned modifiers = 0;
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
	/// Keyed by position. One declaration written in a macro body is expanded once per
	/// call; when the expansions disagree on the type, no single hint is true.
	std::unordered_map<std::string, std::optional<TypeHint>> m_type_hints;
	/// Keyed by range. See add_symbol() for how repeated occurrences merge.
	std::unordered_map<std::string, SymbolToken> m_symbols;
	mutable std::unordered_map<std::string, std::string> m_normalized_files;
	/// Hover text per declaration, keyed by the declared name's range.
	std::unordered_map<std::string, Hover> m_hovers;

public:
	/// Records what hovering the declaration named at `name_range`, or any reference to it,
	/// shows. The first text recorded stays: later passes see the AST lowered further.
	void add_hover(const std::string& file, const SourceRange& name_range, std::string text, std::string parameter = {}) {
		if (file.empty() || !name_range.is_valid() || text.empty()) return;
		auto normalized = normalized_file(file);
		auto key = reference_key(normalized, name_range);
		m_hovers.try_emplace(std::move(key), Hover{
			normalized, name_range, std::move(text), normalized, name_range, {}, std::move(parameter)});
	}

	[[nodiscard]] bool has_hover(const std::string& file, const SourceRange& name_range) const {
		return m_hovers.contains(reference_key(normalized_file(file), name_range));
	}

	/// Carries a range of `file` as this index recorded it onto the text as it is now, or
	/// nothing when the text there was edited. Empty when the text is unchanged.
	using RangeMap = std::function<std::optional<SourceRange>(const SourceRange&)>;

	/// The hover for a zero-based (LSP) position: the declaration a reference there leads
	/// to, or the declaration written there itself. The range is the hovered name. With
	/// `moved`, the position is in a newer text of `file` than this index was built from.
	[[nodiscard]] std::optional<Hover> hover_at(
		const std::string& file, size_t line, size_t character, const RangeMap& moved = {}) const {
		const auto now = [&moved](const SourceRange& range) -> std::optional<SourceRange> {
			return moved ? moved(range) : std::optional(range);
		};

		const ReferenceLink* narrowest = nullptr;
		SourceRange narrowest_range;
		for (const auto& link : m_links) {
			if (link.ref_file != file) continue;
			const auto range = now(link.ref_range);
			if (!range || !covers(*range, line, character)) continue;
			if (!narrowest || is_narrower(*range, narrowest_range)) {
				narrowest = &link;
				narrowest_range = *range;
			}
		}
		// Several links share the narrowest reference when a macro parameter leads both to
		// the parameter and to the argument; the first with a hover answers.
		if (narrowest) {
			for (const auto& link : m_links) {
				if (link.ref_file != file || !same_range(link.ref_range, narrowest->ref_range)) continue;
				const auto found = m_hovers.find(reference_key(link.def_file, link.def_name_range));
				if (found == m_hovers.end()) continue;
				auto hover = found->second;
				hover.file = link.ref_file;
				hover.range = narrowest_range;
				return hover;
			}
		}
		for (const auto& [_, hover] : m_hovers) {
			if (hover.file != file) continue;
			const auto range = now(hover.range);
			if (!range || !covers(*range, line, character)) continue;
			auto moved_hover = hover;
			moved_hover.range = *range;
			return moved_hover;
		}
		return std::nullopt;
	}

	/// Records what the name at `range` stands for. The same range is met more than once -
	/// once per macro expansion, and a declaration both as itself and as a link target - so
	/// the kind first recorded stays, and the modifiers of later records join it.
	void add_symbol(const std::string& file, const SourceRange& range, const SymbolKind kind, const unsigned modifiers = 0) {
		if (!range.is_valid() || range.start.line != range.end.line || range.start.column >= range.end.column) return;
		auto normalized = normalized_file(file);
		auto key = reference_key(normalized, range);
		const auto [existing, inserted] = m_symbols.try_emplace(
			std::move(key), SymbolToken{std::move(normalized), range, kind, modifiers});
		if (!inserted && existing->second.kind == kind) existing->second.modifiers |= modifiers;
	}

	void add_symbol(const Token& token, const SymbolKind kind, unsigned modifiers = 0) {
		if (token.file().empty()) return;
		const auto& name = token.val;
		if (kind == SymbolKind::Parameter && name.size() > 2 && name.front() == '#' && name.back() == '#') {
			modifiers |= SymbolToken::Substitution;
		}
		add_symbol(token.file(), source_range_from_token(token), kind, modifiers);
	}

	/// The symbol occurrences in `file`, in source order.
	[[nodiscard]] std::vector<SymbolToken> symbols_in(const std::string& file) const {
		std::vector<SymbolToken> symbols;
		for (const auto& [_, symbol] : m_symbols) {
			if (symbol.file == file) symbols.push_back(symbol);
		}
		std::ranges::sort(symbols, [](const SymbolToken& left, const SymbolToken& right) {
			return left.range.start.line != right.range.start.line
				? left.range.start.line < right.range.start.line
				: left.range.start.column < right.range.start.column;
		});
		return symbols;
	}

	void add_type_hint(const std::string& file, const SourcePosition& position, std::string type) {
		auto normalized = normalized_file(file);
		const auto key = normalized + "@" + std::to_string(position.line) + ":" + std::to_string(position.column);
		const auto [existing, inserted] = m_type_hints.try_emplace(
			key, TypeHint{std::move(normalized), position, std::move(type)});
		if (!inserted && existing->second && existing->second->type != type) {
			existing->second.reset();
		}
	}

	[[nodiscard]] std::vector<TypeHint> type_hints_in(const std::string& file) const {
		std::vector<TypeHint> hints;
		for (const auto& [_, hint] : m_type_hints) {
			if (hint && hint->file == file) hints.push_back(*hint);
		}
		std::ranges::sort(hints, [](const TypeHint& left, const TypeHint& right) {
			return left.position.line != right.position.line
				? left.position.line < right.position.line
				: left.position.column < right.position.column;
		});
		return hints;
	}

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
		if (reference.file().empty() || declaration.file().empty()) return;
		const auto ref_range = source_range_from_token(reference);
		const auto def_range = source_range_from_token(declaration);
		if (!ref_range.is_valid() || !def_range.is_valid()) return;
		add(reference.file(), ref_range, declaration.file(), def_range);
	}

	/// Same as add_link(), and records both ends as symbols of `kind` for highlighting.
	void add_link(const Token& reference, const Token& declaration, const SymbolKind kind) {
		add_link(reference, declaration);
		add_symbol(reference, kind);
		add_symbol(declaration, kind, SymbolToken::Declaration);
	}

	/// Records a path token -> file link for go-to-definition and document links.
	/// String quotes are excluded from the clickable source range.
	void add_file_link(const Token& path_token, std::string target_file, std::string tooltip = {}) {
		if (path_token.file().empty()) return;
		auto path_range = source_range_from_token(path_token);
		if (path_token.type == token::STRING && path_token.val.size() >= 2) {
			++path_range.start.column;
			--path_range.end.column;
		}
		add_definition_link(
			path_token.file(),
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
