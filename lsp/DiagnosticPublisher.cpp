//
// Created by Mathias Vatter on 08.07.26.
//

#include "DiagnosticPublisher.h"

#include <memory>
#include <utility>

namespace {
	bool diagnostic_belongs_to_source(const Diagnostic& diagnostic, const SourceId& source) {
		if (diagnostic.file.empty()) {
			return true;
		}

		return FileSystemSourceProvider::normalize(diagnostic.file) == source;
	}

	std::string diagnostic_message(const Diagnostic& diagnostic) {
		if (!diagnostic.message.empty()) {
			return diagnostic.message;
		}
		auto message = error_type_to_string(diagnostic.type);
		if (!diagnostic.actual.empty()) {
			message += ": " + diagnostic.actual;
		}
		return message;
	}
} // namespace

void DiagnosticPublisher::publish(const SourceId& entry_source, const std::vector<Diagnostic>& diagnostics) {
	const auto entry = FileSystemSourceProvider::normalize(entry_source.value);
	std::unordered_map<std::string, std::vector<Diagnostic>> diagnostics_by_source;
	std::unordered_set<std::string> affected_sources;
	affected_sources.insert(entry.value);

	for (const auto& diagnostic : diagnostics) {
		const auto source = diagnostic_source(diagnostic, entry);
		diagnostics_by_source[source.value].push_back(diagnostic);
		affected_sources.insert(source.value);
	}

	// Always publish the entry source, even if it has no diagnostics.
	// This clears old diagnostics located directly in the entry file.
	if (!diagnostics_by_source.contains(entry.value)) {
		diagnostics_by_source.try_emplace(entry.value);
	}

	const auto previous_entry = m_diagnostics_by_entry_and_source.find(entry.value);
	if (previous_entry != m_diagnostics_by_entry_and_source.end()) {
		for (const auto& [source_value, _] : previous_entry->second) {
			affected_sources.insert(source_value);
		}
	}

	m_diagnostics_by_entry_and_source[entry.value] = std::move(diagnostics_by_source);

	for (const auto& source : affected_sources) {
		publish_merged_source(SourceId(source));
	}
}

void DiagnosticPublisher::clear_entry(const SourceId& entry_source) {
	const auto entry = FileSystemSourceProvider::normalize(entry_source.value);
	const auto it = m_diagnostics_by_entry_and_source.find(entry.value);
	if (it == m_diagnostics_by_entry_and_source.end()) {
		clear_source(entry_source);
		return;
	}
	std::unordered_set<std::string> affected_sources;
	for (const auto& [source, _] : it->second) {
		affected_sources.insert(source);
	}
	m_diagnostics_by_entry_and_source.erase(it);
	for (const auto& source : affected_sources) {
		publish_merged_source(SourceId(source));
	}
}

void DiagnosticPublisher::discard_entry(const SourceId& entry_source) {
	const auto entry = FileSystemSourceProvider::normalize(entry_source.value);
	const auto it = m_diagnostics_by_entry_and_source.find(entry.value);
	if (it == m_diagnostics_by_entry_and_source.end()) {
		return;
	}
	std::unordered_set<std::string> affected_sources;
	for (const auto& [source, _] : it->second) {
		affected_sources.insert(source);
	}
	m_diagnostics_by_entry_and_source.erase(it);
	for (const auto& source : affected_sources) {
		publish_merged_source(SourceId(source));
	}
}

void DiagnosticPublisher::clear_source(const SourceId& source) {
	const auto normalized_source = FileSystemSourceProvider::normalize(source.value);
	for (auto& [_, diagnostics_by_source] : m_diagnostics_by_entry_and_source) {
		diagnostics_by_source.erase(normalized_source.value);
	}
	publish_source(normalized_source, {});
}

SourceId DiagnosticPublisher::diagnostic_source(const Diagnostic& diagnostic, const SourceId& entry_source) {
	if (diagnostic.file.empty()) {
		return entry_source;
	}
	return FileSystemSourceProvider::normalize(diagnostic.file);
}

std::unique_ptr<JSONObject> DiagnosticPublisher::make_lsp_diagnostic(const Diagnostic& diagnostic) {
	auto result = std::make_unique<JSONObject>();
	result->add("range", diagnostic.range.get_lsp_range());
	result->add("severity", std::make_unique<JSONInt>((int)diagnostic.severity));
	result->add("source", std::make_unique<JSONString>("cksp"));
	result->add("code", std::make_unique<JSONString>(error_type_to_string(diagnostic.type)));
	result->add("message", std::make_unique<JSONString>(diagnostic_message(diagnostic)));
	return result;
}

void DiagnosticPublisher::publish_merged_source(const SourceId& source) const {
	std::vector<Diagnostic> diagnostics;
	for (const auto& [_, diagnostics_by_source] : m_diagnostics_by_entry_and_source) {
		const auto it = diagnostics_by_source.find(source.value);
		if (it == diagnostics_by_source.end()) {
			continue;
		}
		diagnostics.insert(diagnostics.end(), it->second.begin(), it->second.end());
	}
	publish_source(source, diagnostics);
}

void DiagnosticPublisher::publish_source(const SourceId& source, const std::vector<Diagnostic>& diagnostics) const {
	auto items = std::make_unique<JSONArray>();
	for (const auto& diagnostic : diagnostics) {
		if (!diagnostic_belongs_to_source(diagnostic, source)) {
			continue;
		}
		items->add(make_lsp_diagnostic(diagnostic));
	}

	JSONObject params;
	params.add("uri", std::make_unique<JSONString>(uri_from_source(source)));
	params.add("diagnostics", std::move(items));
	m_connection.send_notification("textDocument/publishDiagnostics", params);
}
