#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../cksp/Source/ReferenceIndex.h"
#include "../cksp/Source/SourceProvider.h"

struct ReferenceLocation {
	std::string file;
	SourceRange range;
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
	[[nodiscard]] std::optional<ReferenceLink> resolve_target(
		const std::vector<SourceId>& preferred_entries,
		const SourceId& source,
		size_t line,
		size_t character);
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

	SourceProvider& m_sources;
	std::unordered_map<std::string, State> m_states;
	mutable std::mutex m_mutex;

	[[nodiscard]] bool source_matches_snapshot(const SourceContents& snapshot, const std::string& source);
	[[nodiscard]] std::optional<ReferenceLink> resolve_from_state(
		const State& state,
		const SourceId& source,
		size_t line,
		size_t character);
	[[nodiscard]] std::vector<ReferenceLink> references_from_state(
		const State& state,
		const ReferenceLink& target);
};
