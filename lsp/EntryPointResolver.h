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
	std::optional<SourceId> m_configured_entry;

	std::unordered_set<std::string> m_known_entries;
	std::unordered_map<std::string, ImportGraph> m_import_graphs;

public:
	void set_workspace_root(std::optional<SourceId> workspace_root);
	void set_configured_entry(std::optional<SourceId> configured_entry);
	void register_analysis(const SourceId& entry_source, ImportGraph import_graph);

	void remove_entry(const SourceId& entry_source);
	[[nodiscard]] std::vector<SourceId> affected_entries(const SourceId& changed_source) const;



private:

	[[nodiscard]] std::optional<SourceId> configured_entry_for(const SourceId& source) const;
	[[nodiscard]] bool entry_depends_on(const SourceId& entry, const SourceId& source) const;
	void dump_import_graphs() const;


};
