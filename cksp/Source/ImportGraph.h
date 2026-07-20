//
// Created by Mathias Vatter on 06.07.26.
//

#pragma once

#include <filesystem>
#include <iostream>
#include <ostream>
#include <vector>
#include <unordered_set>

#include "SourceProvider.h"

/**
 * A class tracking the edges of file imports between sources
 *
 * importer -> imported
 *
 * reverse edge:
 *	imported -> dependent
 */
class ImportGraph {
	std::unordered_map<std::string, std::unordered_set<std::string>> m_imports;
	std::unordered_map<std::string, std::unordered_set<std::string>> m_dependents;

public:
	void add_source(const SourceId& source) {
		m_imports.try_emplace(source.value);
		m_dependents.try_emplace(source.value);
	}

	void add_import(const SourceId& importer, const SourceId& imported) {
		add_source(importer);
		add_source(imported);

		m_imports[importer.value].insert(imported.value);
		m_dependents[imported.value].insert(importer.value);
	}

	void remove_import(const SourceId& importer, const SourceId& imported) {
		if (auto it = m_imports.find(importer.value); it != m_imports.end()) {
			it->second.erase(imported.value);
		}
		if (auto it = m_dependents.find(imported.value); it != m_dependents.end()) {
			it->second.erase(importer.value);
		}
	}

	/// file changed on disk and has to be updated
	void set_imports(const SourceId& importer, const std::vector<SourceId>& imported_sources) {
		add_source(importer);
		const auto old_imports = imports_of(importer);
		for (const auto& old_import : old_imports) {
			remove_import(importer, old_import);
		}
		for (const auto& imported : imported_sources) {
			add_import(importer, imported);
		}
	}

	std::vector<SourceId> imports_of(const SourceId& source) const {
		return lookup(m_imports, source);
	}

	std::vector<SourceId> dependents_of(const SourceId& source) const {
		return lookup(m_dependents, source);
	}

	[[nodiscard]] std::vector<SourceId> sources() const {
		std::vector<SourceId> result;
		result.reserve(m_imports.size());
		for (const auto& [source, _] : m_imports) {
			result.emplace_back(source);
		}
		return result;
	}

	/// get all dependents not only immediate
	std::vector<SourceId> transitive_dependents_of(const SourceId& source) const {
		std::vector<SourceId> result;
		std::unordered_set<std::string> visited;
		std::vector<std::string> stack;

		stack.push_back(source.value);
		visited.insert(source.value);

		while (!stack.empty()) {
			auto current = std::move(stack.back());
			stack.pop_back();

			auto it = m_dependents.find(current);
			if (it == m_dependents.end()) {
				continue;
			}
			for (const auto& dependent : it->second) {
				if (!visited.insert(dependent).second) {
					continue;
				}
				result.emplace_back(dependent);
				stack.push_back(dependent);
			}
		}

		return result;
	}

	void print(std::ostream& output = std::cout) const {
		for (const auto& it : m_imports) {
			output << it.first << " --> " << it.second.size() << "\n";
			for (const auto& dependent : it.second) {
				output << "         " << std::filesystem::path(dependent).filename().string() << "\n";
			}
		}
	}

	void print_dot(std::ostream& output, const std::string& graph_name = "imports") const {
		output << "digraph \"" << graph_name << "\" {\n";
		for (const auto& [importer, imported_sources] : m_imports) {
			output << "  \"" << importer << "\";\n";
			for (const auto& imported : imported_sources) {
				output << "  \"" << importer << "\" -> \"" << imported << "\";\n";
			}
		}
		output << "}\n";
	}

private:
	/// lookup a source and return vector
	static std::vector<SourceId> lookup(const std::unordered_map<std::string, std::unordered_set<std::string>>& map, const SourceId& source) {
		std::vector<SourceId> result;
		const auto it = map.find(source.value);
		if (it == map.end()) {
			return result;
		}
		result.reserve(it->second.size());
		for (const auto& res : it->second) {
			result.push_back(SourceId(res));
		}
		return result;
	}


};
