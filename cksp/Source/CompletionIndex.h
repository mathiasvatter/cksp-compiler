//
// Created for LSP completion support.
//

#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../misc/SourceLocation.h"
#include "SourceProvider.h"

/// Subset of the LSP CompletionItemKind enum, using its wire values.
enum class CompletionKind {
	Method = 2,
	Function = 3,
	Field = 5,
	Variable = 6,
	Module = 9,
	Constant = 21,
	Struct = 22,
};

/// One offered item. The three text fields mirror how the cksp-tools extension fills a
/// vscode.CompletionItem: `parameters` goes to labelDetails.detail (right after the label),
/// the category is derived from `kind` into labelDetails.description, and `detail` carries
/// the type annotation or the full signature.
struct CompletionMember {
	std::string label;       ///< what gets inserted: the segment after the last qualifier
	std::string parameters;  ///< "(amount: int, target: int)" for callables, empty otherwise
	std::string detail;      ///< a variable's type, or a callable's full signature
	CompletionKind kind = CompletionKind::Variable;
};

/// A qualifier block's body, used to resolve a shortened qualifier against the
/// scope the cursor sits in. Recorded before desugaring removes the blocks.
struct CompletionScope {
	std::string file;
	SourceRange range;
	std::vector<std::string> path;  ///< container path including the block itself
};

/**
 * Qualifier path -> members, harvested during LSP analysis.
 *
 * A container is anything that can appear left of a dot: a namespace, family or
 * const block (whose path comes from the desugared NodePrefix of its members) or
 * a struct (whose statics are addressed through the struct name). Paths are
 * stored dot-joined in source spelling, e.g. "audio.mixer" or "audio.Envelope".
 *
 * Like ReferenceIndex this is a flat snapshot of copied strings, so it stays
 * valid after the AST that produced it is gone.
 */
class CompletionIndex {
	std::unordered_map<std::string, std::vector<CompletionMember>> m_containers;
	std::vector<CompletionScope> m_scopes;

public:
	void add_scope(std::string file, const SourceRange& range, std::vector<std::string> path) {
		if (file.empty() || path.empty() || !range.is_valid()) return;
		// Token files come from the preprocessor and need not be canonical; lookups
		// arrive normalized from the language server.
		file = FileSystemSourceProvider::normalize(file).value;
		const auto duplicate = std::ranges::find_if(m_scopes, [&](const CompletionScope& existing) {
			return existing.file == file && existing.path == path
				&& existing.range.start.line == range.start.line
				&& existing.range.end.line == range.end.line;
		});
		if (duplicate != m_scopes.end()) return;
		m_scopes.push_back({std::move(file), range, std::move(path)});
	}

	/// Innermost qualifier block containing a zero-based (LSP) position.
	[[nodiscard]] std::vector<std::string> enclosing_path(
		const std::string& file, const size_t line, const size_t character) const {
		const CompletionScope* best = nullptr;
		for (const auto& scope : m_scopes) {
			if (scope.file != file || !covers(scope.range, line, character)) continue;
			// Nested blocks all contain the position; the longest path is the innermost.
			if (!best || scope.path.size() > best->path.size()) best = &scope;
		}
		return best ? best->path : std::vector<std::string>{};
	}

	void add(const std::string& container_path, CompletionMember member) {
		if (container_path.empty() || member.label.empty()) return;
		auto& members = m_containers[container_path];
		// A member can be reached along several traversal paths (a declaration is
		// visited once per enclosing block); first wins.
		const auto duplicate = std::ranges::find_if(
			members,
			[&](const CompletionMember& existing) { return existing.label == member.label; });
		if (duplicate != members.end()) return;
		members.push_back(std::move(member));
	}

	void add(const std::vector<std::string>& container_path, CompletionMember member) {
		add(join(container_path), std::move(member));
	}

	[[nodiscard]] bool empty() const { return m_containers.empty(); }

	/**
	 * Members visible under a typed qualifier chain, resolved lexically.
	 *
	 * A reference may omit leading qualifiers inside its own block (see
	 * DesugarNamespace::add_namespace_prefix), so `mixer.` written inside
	 * `namespace audio` means `audio.mixer`. The enclosing block at the cursor
	 * is walked from innermost outward and the first existing container wins —
	 * the same order the desugaring resolves in, so a nested block shadows an
	 * outer one of the same name.
	 *
	 * Scope ranges come from the snapshot, so an edit that shifted lines can
	 * make the lookup miss. That degrades into the scope-less path below rather
	 * than resolving to the wrong block.
	 */
	[[nodiscard]] std::vector<CompletionMember> members_of(
		const std::vector<std::string>& chain,
		const std::string& file,
		const size_t line,
		const size_t character) const {
		if (chain.empty()) return {};

		auto enclosing = enclosing_path(file, line, character);
		while (true) {
			auto candidate = enclosing;
			candidate.insert(candidate.end(), chain.begin(), chain.end());
			if (const auto found = m_containers.find(join(candidate)); found != m_containers.end()) {
				return sorted(found->second);
			}
			if (enclosing.empty()) break;
			enclosing.pop_back();
		}
		return members_of(chain);
	}

	/**
	 * Members for a chain without cursor context.
	 *
	 * An exact path match wins outright; otherwise every container whose path
	 * *ends* with the chain matches and their members are merged. Used when no
	 * enclosing block resolves the chain — offering a superset beats offering
	 * nothing.
	 */
	[[nodiscard]] std::vector<CompletionMember> members_of(
		const std::vector<std::string>& chain) const {
		if (chain.empty()) return {};

		if (const auto exact = m_containers.find(join(chain)); exact != m_containers.end()) {
			return sorted(exact->second);
		}

		std::vector<CompletionMember> merged;
		std::unordered_set<std::string> seen;
		for (const auto& [path, members] : m_containers) {
			if (!ends_with_chain(path, chain)) continue;
			for (const auto& member : members) {
				if (seen.insert(member.label).second) merged.push_back(member);
			}
		}
		return sorted(merged);
	}

	static std::vector<std::string> split_path(const std::string& path) {
		std::vector<std::string> segments;
		size_t start = 0;
		while (start <= path.size()) {
			const auto dot = path.find('.', start);
			if (dot == std::string::npos) {
				segments.push_back(path.substr(start));
				break;
			}
			segments.push_back(path.substr(start, dot - start));
			start = dot + 1;
		}
		return segments;
	}

	static std::string join(const std::vector<std::string>& segments) {
		std::string path;
		for (const auto& segment : segments) {
			if (!path.empty()) path += '.';
			path += segment;
		}
		return path;
	}

private:
	static std::vector<CompletionMember> sorted(std::vector<CompletionMember> members) {
		// Deterministic order keeps client-side sorting stable and test output diffable.
		std::ranges::sort(members, [](const CompletionMember& left, const CompletionMember& right) {
			return left.label < right.label;
		});
		return members;
	}

	/// True when the zero-based (LSP) position lies inside the one-based range.
	static bool covers(const SourceRange& range, const size_t line, const size_t character) {
		if (!range.is_valid()) return false;
		const size_t start_line = range.start.get_lsp_line();
		const size_t end_line = range.end.get_lsp_line();
		if (line < start_line || line > end_line) return false;
		if (line == start_line && character < range.start.get_lsp_char()) return false;
		if (line == end_line && character > range.end.get_lsp_char()) return false;
		return true;
	}

	static bool ends_with_chain(const std::string& path, const std::vector<std::string>& chain) {
		const auto segments = split_path(path);
		if (segments.size() < chain.size()) return false;
		const size_t offset = segments.size() - chain.size();
		for (size_t i = 0; i < chain.size(); ++i) {
			if (segments[offset + i] != chain[i]) return false;
		}
		return true;
	}
};
