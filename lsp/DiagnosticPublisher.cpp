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
	std::unordered_map<std::string, std::vector<Diagnostic>> diagnostics_by_source;

	for (const auto& diagnostic : diagnostics) {
		const auto source = diagnostic_source(diagnostic, entry_source);
		diagnostics_by_source[source.value].push_back(diagnostic);
	}

	// Always publish the entry source, even if it has no diagnostics.
	// This clears old diagnostics located directly in the entry file.
	if (!diagnostics_by_source.contains(entry_source.value)) {
		diagnostics_by_source.try_emplace(entry_source.value);
	}

	std::unordered_set<std::string> current_sources;
	current_sources.reserve(diagnostics_by_source.size());

	for (const auto& [source_value, source_diagnostics] : diagnostics_by_source) {
		current_sources.insert(source_value);
		publish_source(SourceId(source_value), source_diagnostics);
	}

	auto& previous_sources = m_published_sources_by_entry[entry_source.value];
	for (const auto& previous_source : previous_sources) {
		if (!current_sources.contains(previous_source)) {
			clear_source(SourceId(previous_source));
		}
	}
	previous_sources = std::move(current_sources);
}

void DiagnosticPublisher::clear_entry(const SourceId& entry_source) {
	const auto it = m_published_sources_by_entry.find(entry_source.value);
	if (it == m_published_sources_by_entry.end()) {
		clear_source(entry_source);
		return;
	}
	for (const auto& source : it->second) {
		clear_source(SourceId(source));
	}
	m_published_sources_by_entry.erase(it);
}

void DiagnosticPublisher::clear_source(const SourceId& source) const {
	publish_source(source, {});
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
