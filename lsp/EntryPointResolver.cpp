//
// Created by Mathias Vatter on 08.07.26.
//

#include "EntryPointResolver.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <fstream>
#include <sstream>

void EntryPointResolver::set_workspace_root(std::optional<SourceId> workspace_root) {
    m_workspace_root = std::move(workspace_root);
}

void EntryPointResolver::set_configured_entries(const std::vector<SourceId>& configured_entries) {
    m_configured_entries.clear();
    for (const auto& configured_entry : configured_entries) {
        m_configured_entries.insert(
            FileSystemSourceProvider::normalize(configured_entry.value).value);
    }
}

std::vector<SourceId> EntryPointResolver::register_analysis(
    const SourceId& entry_source, ImportGraph import_graph) {
    const auto entry = FileSystemSourceProvider::normalize(entry_source.value);
    m_known_entries.insert(entry.value);
    m_import_graphs.insert_or_assign(entry.value, std::move(import_graph));

    std::vector<SourceId> subsumed_standalone_entries;
    for (const auto& candidate_value : m_known_entries) {
        const SourceId candidate(candidate_value);
        if (candidate == entry || is_configured_entry(candidate)) {
            continue;
        }
        if (entry_depends_on(entry, candidate)) {
            subsumed_standalone_entries.push_back(candidate);
        }
    }

    if (std::getenv("CKSP_LSP_DUMP_GRAPHS")) {
        dump_import_graphs();
    }
    return subsumed_standalone_entries;
}

void EntryPointResolver::remove_entry(const SourceId& entry_source) {
    const auto entry = FileSystemSourceProvider::normalize(entry_source.value);
    m_known_entries.erase(entry.value);
    m_import_graphs.erase(entry.value);
}

std::vector<SourceId> EntryPointResolver::affected_entries(const SourceId& changed_source) const {
    const auto source = FileSystemSourceProvider::normalize(changed_source.value);

    std::vector<SourceId> entries;
    std::unordered_set<std::string> seen;
    const auto add_entry = [&](const SourceId& entry) {
        if (seen.insert(entry.value).second) {
            entries.push_back(entry);
        }
    };

    // Configured entries own everything reachable from them. Re-analyse every configured
    // entry whose graph contains the changed source.
    bool ownership_is_still_unknown = false;
    if (belongs_to_workspace(source)) {
        for (const auto& configured_value : m_configured_entries) {
            const SourceId configured(configured_value);
            if (configured == source) {
                add_entry(configured);
                continue;
            }

            const auto configured_graph = m_import_graphs.find(configured.value);
            if (configured_graph == m_import_graphs.end()) {
                // Establish all configured graphs before deciding whether an opened source
                // is an orphan. Its temporary standalone snapshot is discarded once an
                // owning entry successfully imports it.
                add_entry(configured);
                ownership_is_still_unknown = true;
            } else if (entry_depends_on(configured, source)) {
                add_entry(configured);
            }
        }
    }
    if (ownership_is_still_unknown) {
        add_entry(source);
    }

    // Any other known entry that transitively imports the source must be refreshed too.
    for (const auto& entry_value : m_known_entries) {
        const SourceId entry(entry_value);
        if (entry == source) {
            continue;
        }
        if (entry_depends_on(entry, source)) {
            add_entry(entry);
        }
    }

    if (!entries.empty()) {
        return entries;
    }

    // Not imported by any known entry: analyse it standalone so orphan files that are
    // not part of any entry point still get diagnostics.
    return {source};
}

bool EntryPointResolver::is_configured_entry(const SourceId& source) const {
    return m_configured_entries.contains(
        FileSystemSourceProvider::normalize(source.value).value);
}

bool EntryPointResolver::is_known_entry(const SourceId& source) const {
    return m_known_entries.contains(FileSystemSourceProvider::normalize(source.value).value);
}

bool EntryPointResolver::is_owned_by_configured_entry(const SourceId& source) const {
    const auto normalized = FileSystemSourceProvider::normalize(source.value);
    for (const auto& configured_value : m_configured_entries) {
        const SourceId configured(configured_value);
        if (normalized == configured || entry_depends_on(configured, normalized)) {
            return true;
        }
    }
    return false;
}

bool EntryPointResolver::belongs_to_workspace(const SourceId& source) const {
    if (!m_workspace_root) {
        return true;
    }

    const auto source_path = std::filesystem::path(source.value);
    const auto root_path = std::filesystem::path(m_workspace_root->value);

    std::error_code error;
    const auto relative = std::filesystem::relative(source_path, root_path, error);
    if (error || relative.empty()) {
        return false;
    }

    const auto first_component = *relative.begin();
    return first_component != "..";
}

bool EntryPointResolver::entry_depends_on(const SourceId& entry, const SourceId& source) const {
    const auto graph = m_import_graphs.find(entry.value);
    if (graph == m_import_graphs.end()) {
        return false;
    }

    const auto dependents = graph->second.transitive_dependents_of(source);
    return std::ranges::any_of(dependents,
        [&entry](const SourceId& dependent) {
            return dependent == entry;
        }
    );
}

void EntryPointResolver::dump_import_graphs() const {
    std::ofstream text("/tmp/cksp-lsp-import-graphs.txt", std::ios::trunc);
    std::ofstream dot("/tmp/cksp-lsp-import-graphs.dot", std::ios::trunc);
    if (dot.is_open()) {
        dot << "digraph \"cksp-lsp-import-graphs\" {\n";
    }

    for (const auto& [entry, graph] : m_import_graphs) {
        if (text.is_open()) {
            text << "Entry: " << entry << "\n";
            graph.print(text);
            text << "\n";
        }

        if (dot.is_open()) {
            dot << "  subgraph \"cluster_" << entry << "\" {\n";
            dot << "    label=\"" << entry << "\";\n";
            for (const auto& source : graph.sources()) {
                dot << "    \"" << source.value << "\";\n";
                for (const auto& imported : graph.imports_of(source)) {
                    dot << "    \"" << source.value << "\" -> \"" << imported.value << "\";\n";
                }
            }
            dot << "  }\n";
        }
    }

    if (dot.is_open()) {
        dot << "}\n";
    }
}
