//
// Created by Mathias Vatter on 08.07.26.
//

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../cksp/Source/ImportGraph.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Tries to resolve the entry point of changed sources in the lsp by
 *  - if an entry point is configured, analyse this one
 *  - if not, go through all entry points and analyse
 *  - is the changed source transitively imported by this entry, analyse this one
 *  - if no entry point found -> analyse the changed source as new entry
 */
class EntryPointResolver {
	std::optional<SourceId> m_workspace_root;
	std::unordered_set<std::string> m_configured_entries;

	std::unordered_set<std::string> m_known_entries;
	std::unordered_map<std::string, ImportGraph> m_import_graphs;

public:
	void set_workspace_root(std::optional<SourceId> workspace_root);
	void set_configured_entries(const std::vector<SourceId>& configured_entries);
	/**
	 * Registers the latest import graph for an entry point.
	 *
	 * Returns previously known non-configured entries that the new graph now imports.
	 * Those entries were analysed standalone before their owning entry was known and
	 * should be discarded by the language server together with their diagnostics.
	 */
	[[nodiscard]] std::vector<SourceId> register_analysis(
		const SourceId& entry_source, ImportGraph import_graph);

	void remove_entry(const SourceId& entry_source);
	[[nodiscard]] std::vector<SourceId> affected_entries(const SourceId& changed_source) const;

	/// True if the source is one of the configured entry points.
	[[nodiscard]] bool is_configured_entry(const SourceId& source) const;

	/// True if the source has been registered as an entry point (configured or standalone).
	[[nodiscard]] bool is_known_entry(const SourceId& source) const;

	/// True if the source is configured or transitively imported by a configured entry.
	/// Such sources are "owned" by configured entries and must not receive
	/// diagnostics from standalone entries that merely include them.
	[[nodiscard]] bool is_owned_by_configured_entry(const SourceId& source) const;



private:

	[[nodiscard]] bool belongs_to_workspace(const SourceId& source) const;
	[[nodiscard]] bool entry_depends_on(const SourceId& entry, const SourceId& source) const;
	void dump_import_graphs() const;


};
