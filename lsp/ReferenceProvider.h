#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "PositionMap.h"
#include "../cksp/Source/ReferenceIndex.h"
#include "../cksp/Source/SourceProvider.h"

struct ReferenceLocation {
	std::string file;
	SourceRange range;
	/// See ReferenceLink::spelled_as_declared - false for a usage that reads as something
	/// else than the declared name, which rename must leave alone.
	bool spelled_as_declared = true;
};

/**
 * Owns the queryable reference snapshots used by LSP navigation.
 *
 * ReferenceIndexBuilder is the AST visitor that harvests reference -> declaration edges.
 * This class operates after that AST is gone: it publishes per-entry snapshots, resolves
 * either end of an edge, performs the reverse lookup for find-references, and layers a
 * partial current analysis over the last successful one without returning stale ranges.
 */
class ReferenceProvider {
public:
	using SourceContents = std::unordered_map<std::string, std::shared_ptr<const std::string>>;

	explicit ReferenceProvider(SourceProvider& sources) : m_sources(sources) {}

	void publish(const SourceId& entry, ReferenceIndex index, bool successful, SourceContents successful_sources);
	void erase(const SourceId& entry);

	[[nodiscard]] bool has_successful_snapshot(const SourceId& entry) const;
	[[nodiscard]] std::vector<DefinitionLink> resolve_definition(
		const std::vector<SourceId>& preferred_entries,
		const SourceId& source,
		size_t line,
		size_t character);
	[[nodiscard]] std::vector<DefinitionLink> document_links(
		const std::vector<SourceId>& preferred_entries,
		const SourceId& source);
	[[nodiscard]] std::optional<ReferenceLink> resolve_target(
		const std::vector<SourceId>& preferred_entries,
		const SourceId& source,
		size_t line,
		size_t character);
	/// Inferred-type hints for `source` from the last successful snapshot. A hint is a bare
	/// position, so when the text has changed since, hints are moved past the edited span
	/// and those inside it dropped rather than left in the wrong place.
	[[nodiscard]] std::vector<TypeHint> type_hints(
		const std::vector<SourceId>& preferred_entries,
		const SourceId& source);
	/// The symbol occurrences in `source` for semantic highlighting, from the last successful
	/// snapshot and moved like type_hints(): a half-typed line keeps the colours around it.
	[[nodiscard]] std::vector<SymbolToken> symbols(
		const std::vector<SourceId>& preferred_entries,
		const SourceId& source);
	[[nodiscard]] std::vector<ReferenceLocation> references_to(
		const ReferenceLink& target,
		bool include_declaration);

private:
	struct State {
		/// Most recent analysis, including a partial index produced by a failed analysis.
		std::shared_ptr<const ReferenceIndex> current;
		/// Last complete index. On success this shares ownership with current.
		std::shared_ptr<const ReferenceIndex> last_successful;
		/// Exact source contents used by last_successful, keyed by normalized path.
		SourceContents last_successful_sources;
	};

	/// A last successful index, with the map onto the current text of the source it was
	/// asked for when that text has changed since.
	struct SuccessfulSnapshot {
		std::shared_ptr<const ReferenceIndex> index;
		std::optional<lsp::PositionMap> moved;
	};

	SourceProvider& m_sources;
	std::unordered_map<std::string, State> m_states;
	mutable std::mutex m_mutex;

	/// The last successful snapshot of the first preferred entry that analysed the source.
	[[nodiscard]] std::optional<SuccessfulSnapshot> successful_snapshot(
		const std::vector<SourceId>& preferred_entries,
		const std::string& normalized_source);
	[[nodiscard]] bool source_matches_snapshot(const SourceContents& snapshot, const std::string& source);
	[[nodiscard]] std::vector<DefinitionLink> resolve_definition_from_state(
		const State& state,
		const SourceId& source,
		size_t line,
		size_t character);
	[[nodiscard]] std::vector<DefinitionLink> document_links_from_state(
		const State& state,
		const SourceId& source);
	[[nodiscard]] std::optional<ReferenceLink> resolve_from_state(
		const State& state,
		const SourceId& source,
		size_t line,
		size_t character);
	[[nodiscard]] std::vector<ReferenceLink> references_from_state(
		const State& state,
		const ReferenceLink& target);
};
