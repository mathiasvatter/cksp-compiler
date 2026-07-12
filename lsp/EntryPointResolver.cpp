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

void EntryPointResolver::set_configured_entry(std::optional<SourceId> configured_entry) {
    if (configured_entry) {
        configured_entry = FileSystemSourceProvider::normalize(configured_entry->value);
    }
    m_configured_entry = std::move(configured_entry);
}

void EntryPointResolver::register_analysis(const SourceId& entry_source, ImportGraph import_graph) {
    const auto entry = FileSystemSourceProvider::normalize(entry_source.value);
    m_known_entries.insert(entry.value);
    m_import_graphs.insert_or_assign(entry.value, std::move(import_graph));
    if (std::getenv("CKSP_LSP_DUMP_GRAPHS")) {
        dump_import_graphs();
    }
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

    // The configured/primary entry owns everything reachable from it. Re-analyse it
    // whenever one of its (transitive) imports changes.
    if (const auto configured = configured_entry_for(source)) {
        if (*configured == source) {
            return {*configured};
        }

        const auto configured_graph = m_import_graphs.find(configured->value);
        if (configured_graph == m_import_graphs.end()) {
            // Ownership is not known yet: analyse the configured entry to establish it,
            // and also analyse the source standalone in case it turns out to be an orphan.
            // If it is actually owned, its standalone diagnostics are suppressed at publish
            // time and the standalone entry is dropped on the next change.
            add_entry(*configured);
            add_entry(source);
        } else if (entry_depends_on(*configured, source)) {
            add_entry(*configured);
        }
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
    if (!m_configured_entry) {
        return false;
    }
    return FileSystemSourceProvider::normalize(source.value) == *m_configured_entry;
}

bool EntryPointResolver::is_known_entry(const SourceId& source) const {
    return m_known_entries.contains(FileSystemSourceProvider::normalize(source.value).value);
}

bool EntryPointResolver::is_owned_by_configured_entry(const SourceId& source) const {
    if (!m_configured_entry) {
        return false;
    }
    const auto normalized = FileSystemSourceProvider::normalize(source.value);
    if (normalized == *m_configured_entry) {
        return true;
    }
    return entry_depends_on(*m_configured_entry, normalized);
}

std::optional<SourceId> EntryPointResolver::configured_entry_for(const SourceId& source) const {
    if (!m_configured_entry) {
        return std::nullopt;
    }

    if (!m_workspace_root) {
        return m_configured_entry;
    }

    const auto source_path = std::filesystem::path(source.value);
    const auto root_path = std::filesystem::path(m_workspace_root->value);

    std::error_code error;
    const auto relative = std::filesystem::relative(source_path, root_path, error);
    if (error || relative.empty()) {
        return std::nullopt;
    }

    const auto first_component = *relative.begin();
    if (first_component == "..") {
        return std::nullopt;
    }

    return m_configured_entry;
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
