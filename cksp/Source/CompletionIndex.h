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
	/// Struct this member is itself an instance of, so a chain can walk on through it
	/// (<inst.child.>). Index-internal; never serialized.
	std::string object_type;
};

/// A named declaration the user can write, with the scope it is visible in.
///
/// CKSP hoists declarations in callbacks to the global scope, so the only construct
/// that hides a name is a function body. `scope` is therefore either invalid (global)
/// or the range of the enclosing function.
struct CompletionDeclaration {
	std::string name;         ///< qualified name after desugaring, e.g. "audio.inst"
	std::string object_type;  ///< struct it is an instance of, empty for non-objects
	std::string parameters;   ///< "(a: int)" for callables
	std::string detail;       ///< type annotation or full signature
	CompletionKind kind = CompletionKind::Variable;
	std::string file;
	SourceRange scope;
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
	/// Struct name -> members reachable through an *instance* of it.
	std::unordered_map<std::string, std::vector<CompletionMember>> m_type_members;
	std::vector<CompletionScope> m_scopes;
	std::vector<CompletionDeclaration> m_declarations;
	/// Method bodies, so <self> resolves to the struct the cursor is inside of.
	std::vector<CompletionScope> m_self_scopes;
	/// Kind of each qualifier block, so it can be offered with the right icon.
	std::unordered_map<std::string, CompletionKind> m_container_kinds;

public:
	void add_type_member(const std::string& struct_name, CompletionMember member) {
		if (struct_name.empty() || member.label.empty()) return;
		auto& members = m_type_members[struct_name];
		const auto duplicate = std::ranges::find_if(
			members,
			[&](const CompletionMember& existing) { return existing.label == member.label; });
		if (duplicate != members.end()) return;
		members.push_back(std::move(member));
	}

	void add_declaration(CompletionDeclaration declaration) {
		if (declaration.name.empty()) return;
		declaration.file = declaration.file.empty()
			? std::string{}
			: FileSystemSourceProvider::normalize(declaration.file).value;
		m_declarations.push_back(std::move(declaration));
	}

	/// Records a method body so <self.> inside it resolves to `struct_name`.
	void add_self_scope(std::string file, const SourceRange& range, std::string struct_name) {
		if (file.empty() || struct_name.empty() || !range.is_valid()) return;
		m_self_scopes.push_back({
			FileSystemSourceProvider::normalize(file).value, range, {std::move(struct_name)}
		});
	}

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

	[[nodiscard]] bool empty() const {
		return m_containers.empty() && m_type_members.empty() && m_declarations.empty();
	}

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

	/**
	 * Members reachable through an instance, for a chain the qualifier lookup did not
	 * resolve: <inst.>, <audio.inst.>, <inst.child.> or <self.>.
	 *
	 * The longest leading run of the chain that names a declaration wins - a declaration
	 * inside a namespace carries the namespace in its name, so the run can span several
	 * segments. Whatever follows is walked member by member through the struct types.
	 */
	[[nodiscard]] std::vector<CompletionMember> instance_members_of(
		const std::vector<std::string>& chain,
		const std::string& file,
		const size_t line,
		const size_t character) const {
		if (chain.empty()) return {};

		// <self> is not a declaration one can look up by name: the struct desugaring gives
		// it the struct's own token. Resolve it from the method body the cursor sits in.
		size_t consumed = 1;
		auto type = chain[0] == "self"
			? enclosing_struct(file, line, character)
			: resolve_declaration_type(chain, file, line, character, consumed);
		if (type.empty()) return {};

		for (size_t i = consumed; i < chain.size(); ++i) {
			type = member_type(type, chain[i]);
			if (type.empty()) return {};
		}
		const auto members = m_type_members.find(type);
		if (members == m_type_members.end()) return {};
		return sorted(members->second);
	}

	/**
	 * Everything nameable at a position without a qualifier.
	 *
	 * A function body is the only construct in CKSP that hides a name, so the set is
	 * the globals plus the locals of the enclosing function. Names are offered the way
	 * they have to be written there: inside `namespace audio` the declaration
	 * <audio.rate> is offered as <rate>, outside it as <audio.rate>.
	 */
	[[nodiscard]] std::vector<CompletionMember> visible_symbols(
		const std::string& file, const size_t line, const size_t character) const {
		const auto enclosing = enclosing_path(file, line, character);
		const auto prefix = enclosing.empty() ? std::string{} : join(enclosing) + ".";

		std::vector<CompletionMember> found;
		std::unordered_set<std::string> seen;
		const auto offer = [&](std::string label, const CompletionDeclaration& declaration) {
			if (label.empty() || !seen.insert(label).second) return;
			found.push_back({
				std::move(label), declaration.parameters, declaration.detail, declaration.kind,
			});
		};

		for (const auto& declaration : m_declarations) {
			if (!is_visible(declaration, file, line, character)) continue;
			if (!is_nameable(declaration.name)) continue;
			// Inside its own block a declaration is written without the qualifiers.
			if (!prefix.empty() && declaration.name.starts_with(prefix)) {
				offer(declaration.name.substr(prefix.size()), declaration);
			} else {
				offer(declaration.name, declaration);
			}
		}

		// Qualifier blocks themselves: at global scope their first segment, inside one
		// the next segment down.
		for (const auto& [path, _] : m_containers) {
			const auto segments = split_path(path);
			if (segments.size() <= enclosing.size()) continue;
			if (!std::equal(enclosing.begin(), enclosing.end(), segments.begin())) continue;
			const auto& label = segments[enclosing.size()];
			if (label.empty() || !seen.insert(label).second) continue;
			const auto kind = m_container_kinds.find(join(
				std::vector(segments.begin(), segments.begin() + enclosing.size() + 1)));
			found.push_back({
				label, {}, {},
				kind == m_container_kinds.end() ? CompletionKind::Module : kind->second,
			});
		}
		return sorted(std::move(found));
	}

	void set_container_kind(const std::string& path, const CompletionKind kind) {
		if (!path.empty()) m_container_kinds.emplace(path, kind);
	}

	/// Innermost method body containing the position, as a struct name.
	[[nodiscard]] std::string enclosing_struct(
		const std::string& file, const size_t line, const size_t character) const {
		const CompletionScope* best = nullptr;
		for (const auto& scope : m_self_scopes) {
			if (scope.file != file || !covers(scope.range, line, character)) continue;
			if (!best || is_narrower(scope.range, best->range)) best = &scope;
		}
		return best && !best->path.empty() ? best->path.front() : std::string{};
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

	/// A declaration is visible when it is global, or local to the function the cursor
	/// is in.
	[[nodiscard]] static bool is_visible(
		const CompletionDeclaration& declaration,
		const std::string& file,
		const size_t line,
		const size_t character) {
		if (!declaration.scope.is_valid()) return true;
		return declaration.file == file && covers(declaration.scope, line, character);
	}

	/// Struct members carry the OBJ_DELIMITER and are only reachable through an
	/// instance; compiler-generated names are not something the user can write.
	[[nodiscard]] static bool is_nameable(const std::string& name) {
		if (name.empty() || name == "self" || name == "_") return false;
		if (name.starts_with("__") || name.starts_with('$')) return false;
		return name.find("::") == std::string::npos;
	}

	/// Longest leading run of `chain` that names a declaration, as its struct type.
	/// `consumed` receives how many segments that run spans.
	[[nodiscard]] std::string resolve_declaration_type(
		const std::vector<std::string>& chain,
		const std::string& file,
		const size_t line,
		const size_t character,
		size_t& consumed) const {
		const auto enclosing = enclosing_path(file, line, character);
		std::string best_type;
		std::string qualified;
		for (size_t count = 1; count <= chain.size(); ++count) {
			if (count > 1) qualified += ".";
			qualified += chain[count - 1];
			// Written inside a namespace, a declaration may be named without its
			// qualifiers, exactly like a container reference.
			auto prefix = enclosing;
			while (true) {
				auto candidate = prefix;
				candidate.push_back(qualified);
				if (const auto type = declaration_type(join(candidate), file, line, character);
					!type.empty()) {
					best_type = type;
					consumed = count;
					break;
				}
				if (prefix.empty()) break;
				prefix.pop_back();
			}
		}
		return best_type;
	}

	/// Struct type of the declaration named `name` that is visible at the position.
	/// A function-scoped declaration shadows a global one of the same name.
	[[nodiscard]] std::string declaration_type(
		const std::string& name,
		const std::string& file,
		const size_t line,
		const size_t character) const {
		const CompletionDeclaration* best = nullptr;
		for (const auto& declaration : m_declarations) {
			if (declaration.name != name) continue;
			const bool is_local = declaration.scope.is_valid();
			if (is_local
				&& (declaration.file != file || !covers(declaration.scope, line, character))) {
				continue;
			}
			if (!best || (is_local && !best->scope.is_valid())) best = &declaration;
		}
		return best ? best->object_type : std::string{};
	}

	/// Struct type of `member` inside `struct_name`, empty when it is not an object.
	[[nodiscard]] std::string member_type(
		const std::string& struct_name, const std::string& member) const {
		const auto members = m_type_members.find(struct_name);
		if (members == m_type_members.end()) return {};
		for (const auto& entry : members->second) {
			if (entry.label == member) return entry.object_type;
		}
		return {};
	}

	static bool is_narrower(const SourceRange& left, const SourceRange& right) {
		const size_t left_lines = left.end.get_lsp_line() - left.start.get_lsp_line();
		const size_t right_lines = right.end.get_lsp_line() - right.start.get_lsp_line();
		return left_lines < right_lines;
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
