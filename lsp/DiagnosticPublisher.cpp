//
// Created by Mathias Vatter on 08.07.26.
//

#include "DiagnosticPublisher.h"

#include <memory>
#include <utility>

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

bool DiagnosticPublisher::same_published_diagnostic(
	const Diagnostic& left,
	const Diagnostic& right) {
	return left.type == right.type
		&& left.severity == right.severity
		&& left.display_message() == right.display_message()
		&& left.range == right.range
		&& left.migration_kind == right.migration_kind
		&& left.fix == right.fix;
}

std::unique_ptr<JSONObject> DiagnosticPublisher::make_lsp_edit_data(
	const Diagnostic::DiagnosticFix::Edit& edit) {
	auto edit_range = std::make_unique<JSONObject>();
	switch (edit.kind) {
		case Diagnostic::DiagnosticFix::EditKind::InsertBefore:
			edit_range->add("start", edit.range.start.get_lsp_position());
			edit_range->add("end", edit.range.start.get_lsp_position());
			break;
		case Diagnostic::DiagnosticFix::EditKind::InsertAfter:
			edit_range->add("start", edit.range.end.get_lsp_position());
			edit_range->add("end", edit.range.end.get_lsp_position());
			break;
		case Diagnostic::DiagnosticFix::EditKind::Replace:
			edit_range = edit.range.get_lsp_range();
			break;
	}

	auto data = std::make_unique<JSONObject>();
	data->add("targetUri", std::make_unique<JSONString>(uri_from_source(SourceId(edit.file))));
	data->add("range", std::move(edit_range));
	data->add("newText", std::make_unique<JSONString>(edit.new_text));
	return data;
}

std::unique_ptr<JSONObject> DiagnosticPublisher::make_lsp_fix_data(
	const Diagnostic::DiagnosticFix& fix) {
	auto data = std::make_unique<JSONObject>();
	data->add("fixKind",std::make_unique<JSONString>(Diagnostic::fix_kind_to_string(fix.kind)));
	data->add("title", std::make_unique<JSONString>(fix.title));
	data->add("isPreferred", std::make_unique<JSONBool>(fix.is_preferred));
	auto edits = std::make_unique<JSONArray>();
	for (const auto& edit : fix.edits) {
		edits->add(make_lsp_edit_data(edit));
	}
	data->add("edits", std::move(edits));
	return data;
}

std::unique_ptr<JSONObject> DiagnosticPublisher::make_lsp_diagnostic(const Diagnostic& diagnostic) {
	auto result = std::make_unique<JSONObject>();
	result->add("range", diagnostic.range.get_lsp_range());
	result->add("severity", std::make_unique<JSONInt>((int)diagnostic.severity));
	result->add("source", std::make_unique<JSONString>("cksp"));
	result->add("code", std::make_unique<JSONString>(error_type_to_string(diagnostic.type)));
	// The console sink prints <Expected>/<Got> as their own lines; the editor only ever gets
	// `message`, so without this the parser's most useful half - what it wanted and what it
	// found - never reaches the user. That is what makes a message like "Found unknown
	// expression token." unreadable while typing.
	auto message = diagnostic.display_message();
	if (const auto detail = diagnostic.display_detail(); !detail.empty()) {
		message += " " + detail;
	}
	result->add("message", std::make_unique<JSONString>(message));

	// A token a macro or define substitution produced is reported wherever the substitution
	// left it - the call site for a body word, the <define> declaration for a define usage.
	// Neither is where the user wrote it, so the spelling the source actually holds is
	// attached as related information; the client renders it as a second, clickable
	// location under the message.
	if (diagnostic.expansion && !diagnostic.expansion->file.empty()) {
		const auto& expansion = *diagnostic.expansion;
		auto location = std::make_unique<JSONObject>();
		location->add("uri", std::make_unique<JSONString>(
			uri_from_source(SourceId(expansion.file))));
		location->add("range", expansion.range.get_lsp_range());

		auto entry = std::make_unique<JSONObject>();
		entry->add("location", std::move(location));
		entry->add("message", std::make_unique<JSONString>(
			"substituted from <" + expansion.spelling + ">"));

		auto related = std::make_unique<JSONArray>();
		related->add(std::move(entry));
		result->add("relatedInformation", std::move(related));
	}

	if (diagnostic.fix || diagnostic.migration_kind) {
		auto data = diagnostic.fix
			? make_lsp_fix_data(*diagnostic.fix)
			: std::make_unique<JSONObject>();
		if (diagnostic.migration_kind) {
			data->add(
				"migrationKind",
				std::make_unique<JSONString>(
					Diagnostic::migration_kind_to_string(*diagnostic.migration_kind))
			);
		}
		result->add("data", std::move(data));
	}
	return result;
}

void DiagnosticPublisher::publish_merged_source(const SourceId& source) const {
	// A file that is owned by a configured entry receives diagnostics only from
	// configured entries. Standalone entries that merely include it as a shared
	// dependency (e.g. an opened sibling script) must not contribute, otherwise their
	// out-of-context analysis leaks false diagnostics onto the shared file.
	const bool owned_by_configured = m_entries && m_entries->is_owned_by_configured_entry(source);

	std::vector<Diagnostic> diagnostics;
	for (const auto& [entry, diagnostics_by_source] : m_diagnostics_by_entry_and_source) {
		if (owned_by_configured && !(m_entries && m_entries->is_configured_entry(SourceId(entry)))) {
			continue;
		}
		const auto it = diagnostics_by_source.find(source.value);
		if (it == diagnostics_by_source.end()) {
			continue;
		}
		for (const auto& diagnostic : it->second) {
			bool already_published = false;
			for (const auto& existing : diagnostics) {
				if (same_published_diagnostic(existing, diagnostic)) {
					already_published = true;
					break;
				}
			}
			if (!already_published) {
				diagnostics.push_back(diagnostic);
			}
		}
	}
	publish_source(source, diagnostics);
}

void DiagnosticPublisher::publish_source(const SourceId& source, const std::vector<Diagnostic>& diagnostics) const {
	auto items = std::make_unique<JSONArray>();
	for (const auto& diagnostic : diagnostics) {
		if (diagnostic_source(diagnostic, source) != source) {
			continue;
		}
		items->add(make_lsp_diagnostic(diagnostic));
	}

	JSONObject params;
	params.add("uri", std::make_unique<JSONString>(uri_from_source(source)));
	params.add("diagnostics", std::move(items));
	m_connection.send_notification("textDocument/publishDiagnostics", params);
}
