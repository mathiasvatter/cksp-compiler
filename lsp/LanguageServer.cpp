//
// Created by Mathias Vatter on 07.07.26.
//

#include "LanguageServer.h"

#include "RequestParams.h"
#include "TrackingSourceProvider.h"
#include "../cksp/Compiler.h"
#include "../misc/DiagnosticSink.h"
#include "version.h"

#include <filesystem>
#include <unordered_set>

using lsp::object_at;
using lsp::string_at;
using lsp::position_params;
using lsp::source_from_optional_uri_or_path;
using lsp::resolve_configured_entry;

LanguageServer::~LanguageServer() {
	stop_analysis_worker();
}

int LanguageServer::run() {
	while (auto message = m_connection.read_message()) {
		handle_message(*message);
		if (m_exit_requested) {
			break;
		}
	}
	stop_analysis_worker();
	return 0;
}

void LanguageServer::handle_message(const JsonRpcMessage& message) {
	if (message.is_request()) {
		handle_request(message);
		return;
	}
	if (message.is_notification()) {
		handle_notification(message);
		return;
	}
}

void LanguageServer::handle_request(const JsonRpcMessage& message) {
	const auto* method = message.method();
	if (!method) return;

	if (method->value == "initialize") {
		handle_initialize(message);
	} else if (method->value == "shutdown") {
		handle_shutdown(message);
	} else if (method->value == "textDocument/definition") {
		handle_definition(message);
	} else if (method->value == "textDocument/references") {
		handle_references(message);
	} else if (method->value == "textDocument/prepareRename") {
		handle_prepare_rename(message);
	} else if (method->value == "textDocument/rename") {
		handle_rename(message);
	} else if (method->value == "textDocument/documentHighlight") {
		handle_document_highlight(message);
	}
}

void LanguageServer::handle_notification(const JsonRpcMessage& message) {
	const auto* method = message.method();
	if (!method) return;

	if (method->value == "initialized") {
		m_initialized = true;
	} else if (method->value == "textDocument/didOpen") {
		handle_did_open(message);
	} else if (method->value == "textDocument/didChange") {
		handle_did_change(message);
	} else if (method->value == "textDocument/didClose") {
		handle_did_close(message);
	} else if (method->value == "workspace/didChangeWatchedFiles") {
		handle_did_change_watched_files(message);
	} else if (method->value == "exit") {
		m_exit_requested = true;
	}
}

void LanguageServer::schedule_analysis_for_source(const SourceId& changed_source) {
	const auto source = FileSystemSourceProvider::normalize(changed_source.value);
	{
		std::lock_guard lock(m_analysis_mutex);
		m_pending_analysis_sources.insert(source.value);
		++m_analysis_generation;
	}
	m_analysis_cv.notify_one();
}

void LanguageServer::analysis_worker_loop() {
	while (true) {
		std::vector<SourceId> changed_sources;
		uint64_t generation = 0;
		{
			std::unique_lock lock(m_analysis_mutex);
			m_analysis_cv.wait(lock, [this] {
				return m_stop_analysis_worker || !m_pending_analysis_sources.empty();
			});
			if (m_stop_analysis_worker) {
				return;
			}

			while (true) {
				const auto observed_generation = m_analysis_generation;
				const bool changed = m_analysis_cv.wait_for(lock, ANALYSIS_DEBOUNCE, [this, observed_generation] {
					return m_stop_analysis_worker || m_analysis_generation != observed_generation;
				});
				if (m_stop_analysis_worker) {
					return;
				}
				if (!changed) {
					break;
				}
			}

			generation = m_analysis_generation;
			changed_sources.reserve(m_pending_analysis_sources.size());
			for (const auto& source : m_pending_analysis_sources) {
				changed_sources.emplace_back(source);
			}
			m_pending_analysis_sources.clear();
		}

		analyze_entries_for_sources(changed_sources, generation);
	}
}

void LanguageServer::stop_analysis_worker() {
	{
		std::lock_guard lock(m_analysis_mutex);
		m_stop_analysis_worker = true;
	}
	m_analysis_cv.notify_one();
	if (m_analysis_worker.joinable()) {
		m_analysis_worker.join();
	}
}

bool LanguageServer::is_analysis_current(const uint64_t generation) const {
	std::lock_guard lock(m_analysis_mutex);
	return !m_stop_analysis_worker && generation == m_analysis_generation;
}

void LanguageServer::analyze_entry(const SourceId& entry_source) {
	CollectingDiagnosticSink diagnostics;
	auto config = std::make_unique<CompilerConfig>();
	config->lsp = true;
	TrackingSourceProvider analysis_sources(m_sources);
	Compiler compiler(std::move(config), analysis_sources);
	const auto result = compiler.analyze(entry_source, diagnostics);
	auto successful_sources = result.success
		? analysis_sources.take_loaded_contents()
		: ReferenceProvider::SourceContents{};
	// A completed analysis is published even when newer input arrived meanwhile: it is a
	// consistent snapshot and strictly better than nothing, and the pending re-analysis
	// supersedes it right after. Only a stopping worker must not publish.
	{
		std::lock_guard lock(m_analysis_mutex);
		if (m_stop_analysis_worker) {
			return;
		}
	}
	std::lock_guard lock(m_state_mutex);
	const auto entry = FileSystemSourceProvider::normalize(entry_source.value);
	// A filesystem notification may arrive while this analysis is running. Do not let
	// its now-stale result resurrect an entry that has already been removed.
	if (m_deleted_sources.contains(entry.value)) {
		return;
	}
	const bool has_successful_graph = m_references.has_successful_snapshot(entry);
	// A failed analysis often has only a partial import graph. Use it to establish a new
	// entry, but never replace graph ownership learned from a complete analysis.
	if (result.success || !has_successful_graph) {
		m_entry_points.register_analysis(entry, compiler.import_graph());
	}

	m_references.publish(entry, compiler.reference_index(), result.success, std::move(successful_sources));
	m_diagnostic_publisher.publish(entry_source, diagnostics.diagnostics());
}

void LanguageServer::mark_source_available(const SourceId& source) {
	std::lock_guard lock(m_state_mutex);
	m_deleted_sources.erase(FileSystemSourceProvider::normalize(source.value).value);
}

void LanguageServer::handle_deleted_source(const SourceId& deleted_source) {
	const auto source = FileSystemSourceProvider::normalize(deleted_source.value);
	std::vector<SourceId> affected_entries;
	{
		std::lock_guard lock(m_state_mutex);
		m_deleted_sources.insert(source.value);
		affected_entries = m_entry_points.affected_entries(source);

		if (m_entry_points.is_known_entry(source)) {
			m_entry_points.remove_entry(source);
			m_diagnostic_publisher.discard_entry(source);
			m_references.erase(source);
		}
		// Diagnostics located in the removed file must disappear immediately, including
		// diagnostics owned by an importing entry point.
		m_diagnostic_publisher.clear_source(source);
	}

	// Resolve dependents before removing the deleted entry above, then schedule the
	// surviving entries directly. The deleted source itself must not be analyzed as a
	// new standalone entry.
	for (const auto& entry : affected_entries) {
		if (FileSystemSourceProvider::normalize(entry.value) != source) {
			schedule_analysis_for_source(entry);
		}
	}
}

void LanguageServer::analyze_entries_for_sources(const std::vector<SourceId>& changed_sources, const uint64_t generation) {
	std::unordered_set<std::string> seen_entries;
	while (true) {
		std::vector<SourceId> entries;
		{
			std::lock_guard lock(m_state_mutex);
			for (const auto& changed_source : changed_sources) {
				const auto normalized_source = FileSystemSourceProvider::normalize(changed_source.value);
				const auto affected_entries = m_entry_points.affected_entries(normalized_source);
				bool source_is_entry = false;
				for (const auto& entry : affected_entries) {
					if (entry == normalized_source) {
						source_is_entry = true;
					}
					if (seen_entries.contains(entry.value)) {
						continue;
					}
					if (seen_entries.insert(entry.value).second) {
						entries.push_back(entry);
					}
				}
				if (!source_is_entry) {
					m_entry_points.remove_entry(normalized_source);
					m_diagnostic_publisher.discard_entry(normalized_source);
					m_references.erase(normalized_source);
				}
			}
		}

		if (entries.empty()) {
			return;
		}

		for (size_t i = 0; i < entries.size(); ++i) {
			if (!is_analysis_current(generation)) {
				// Newer input arrived: stop the batch so it is picked up quickly, but hand
				// the not-yet-analyzed entries back to the queue. Otherwise their didOpen
				// is consumed without effect and e.g. an opened standalone entry would
				// never be analyzed at all.
				std::lock_guard lock(m_analysis_mutex);
				for (size_t remaining = i; remaining < entries.size(); ++remaining) {
					m_pending_analysis_sources.insert(entries[remaining].value);
				}
				return;
			}
			analyze_entry(entries[i]);
		}
	}
}

void LanguageServer::handle_initialize(const JsonRpcMessage& message) {
	m_initialized = true;
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	m_workspace_root = source_from_optional_uri_or_path(params, "rootUri", "rootPath");
	if (m_workspace_root) {
		m_workspace_root = FileSystemSourceProvider::normalize(m_workspace_root->value);
	}
	m_configured_entry_source = resolve_configured_entry(params, m_workspace_root);
	{
		std::lock_guard lock(m_state_mutex);
		m_entry_points.set_workspace_root(m_workspace_root);
		m_entry_points.set_configured_entry(m_configured_entry_source);
	}

	// Analyse the configured entry up front so project-wide diagnostics are available
	// without having to open one of its member files first, and so ownership of shared
	// files is established early.
	if (m_configured_entry_source) {
		schedule_analysis_for_source(*m_configured_entry_source);
	}

	JSONObject sync;
	sync.add("openClose", std::make_unique<JSONBool>(true));
	sync.add("change", std::make_unique<JSONInt>(1));

	JSONObject rename_options;
	rename_options.add("prepareProvider", std::make_unique<JSONBool>(true));

	JSONObject capabilities;
	capabilities.add("textDocumentSync", std::make_unique<JSONObject>(sync));
	capabilities.add("definitionProvider", std::make_unique<JSONBool>(true));
	capabilities.add("referencesProvider", std::make_unique<JSONBool>(true));
	capabilities.add("renameProvider", std::make_unique<JSONObject>(rename_options));
	capabilities.add("documentHighlightProvider", std::make_unique<JSONBool>(true));

	JSONObject server_info;
	server_info.add("name", std::make_unique<JSONString>("cksp-lsp"));
	server_info.add("version", std::make_unique<JSONString>(COMPILER_VERSION));

	JSONObject result;
	result.add("capabilities", std::make_unique<JSONObject>(capabilities));
	result.add("serverInfo", std::make_unique<JSONObject>(server_info));

	if (const auto* id = message.id()) {
		m_connection.send_response(*id, result);
	}
}

void LanguageServer::handle_shutdown(const JsonRpcMessage& message) {
	m_shutdown_requested = true;
	if (const auto* id = message.id()) {
		m_connection.send_response(*id, JSONNull{});
	}
}

std::optional<ReferenceLink> LanguageServer::resolve_navigation_target(
	const JsonRpcMessage& message) {
	const auto position = position_params(message);
	if (!position) return std::nullopt;
	return resolve_target_at(position->source, position->line, position->character);
}

std::optional<ReferenceLink> LanguageServer::resolve_target_at(
	const SourceId& source, const size_t line, const size_t character) {
	std::lock_guard lock(m_state_mutex);
	return m_references.resolve_target(
		m_entry_points.affected_entries(source), source, line, character);
}

void LanguageServer::handle_definition(const JsonRpcMessage& message) {
	const auto found = resolve_navigation_target(message);

	const auto* id = message.id();
	if (!id) return;
	if (found) {
		JSONObject location;
		location.add("uri", std::make_unique<JSONString>(uri_from_source(SourceId(found->def_file))));
		location.add("range", found->def_range.get_lsp_range());
		m_connection.send_response(*id, location);
	} else {
		m_connection.send_response(*id, JSONNull{});
	}
}

void LanguageServer::handle_references(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* context = object_at(params, "context");
	const auto* include_declaration_value = context ? context->get<JSONBool>("includeDeclaration") : nullptr;
	const bool include_declaration = include_declaration_value && include_declaration_value->value;

	std::vector<ReferenceLocation> locations;

	if (const auto target = resolve_navigation_target(message)) {
		locations = m_references.references_to(*target, include_declaration);
	}

	JSONArray result;
	for (const auto& found : locations) {
		auto location = std::make_unique<JSONObject>();
		location->add("uri", std::make_unique<JSONString>(uri_from_source(SourceId(found.file))));
		location->add("range", found.range.get_lsp_range());
		result.add(std::move(location));
	}
	if (const auto* id = message.id()) {
		m_connection.send_response(*id, result);
	}
}

void LanguageServer::handle_prepare_rename(const JsonRpcMessage& message) {
	const auto* id = message.id();
	if (!id) return;

	const auto position = position_params(message);
	const auto target = position
		? resolve_target_at(position->source, position->line, position->character)
		: std::nullopt;
	if (!position || !target) {
		// null makes the client refuse the rename ("element can't be renamed"),
		// e.g. on builtins, keywords or whitespace
		m_connection.send_response(*id, JSONNull{});
		return;
	}

	const auto symbol_range = RenameProvider::renameable_range(
		*target, position->source, position->line, position->character);
	if (!symbol_range) {
		m_connection.send_response(*id, JSONNull{});
		return;
	}
	m_connection.send_response(*id, *symbol_range->get_lsp_range());
}

void LanguageServer::handle_rename(const JsonRpcMessage& message) {
	const auto* id = message.id();
	if (!id) return;

	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* new_name = string_at(params, "newName");
	const auto position = position_params(message);
	if (!position || !new_name) {
		m_connection.send_error_response(*id, -32602, "Invalid rename parameters.");
		return;
	}

	const auto target = resolve_target_at(position->source, position->line, position->character);
	if (!target) {
		m_connection.send_error_response(*id, -32803, "No renamable symbol at this position.");
		return;
	}

	const auto result = m_rename.rename(*target, new_name->value);
	if (!result.ok()) {
		m_connection.send_error_response(*id, -32803, result.error);
		return;
	}

	auto changes = std::make_unique<JSONObject>();
	std::unordered_map<std::string, JSONArray*> edits_by_file;
	for (const auto& location : result.edits) {
		auto& edits = edits_by_file[location.file];
		if (!edits) {
			auto array = std::make_unique<JSONArray>();
			edits = array.get();
			changes->add(uri_from_source(SourceId(location.file)), std::move(array));
		}
		auto edit = std::make_unique<JSONObject>();
		edit->add("range", location.range.get_lsp_range());
		edit->add("newText", std::make_unique<JSONString>(new_name->value));
		edits->add(std::move(edit));
	}

	JSONObject workspace_edit;
	workspace_edit.add("changes", std::move(changes));
	m_connection.send_response(*id, workspace_edit);
}

void LanguageServer::handle_document_highlight(const JsonRpcMessage& message) {
	const auto* id = message.id();
	if (!id) return;

	JSONArray highlights;
	const auto position = position_params(message);
	const auto target = position
		? resolve_target_at(position->source, position->line, position->character)
		: std::nullopt;
	if (target) {
		for (const auto& location : m_references.references_to(*target, true)) {
			if (location.file != position->source.value) continue;
			auto highlight = std::make_unique<JSONObject>();
			highlight->add("range", location.range.get_lsp_range());
			highlights.add(std::move(highlight));
		}
	}
	m_connection.send_response(*id, highlights);
}

void LanguageServer::handle_did_open(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = object_at(params, "textDocument");
	const auto* uri = string_at(text_document, "uri");
	const auto* text = string_at(text_document, "text");
	if (!uri || !text) return;

	const auto version = text_document->get_int("version").value_or(0);
	const auto source = source_from_uri(uri->value);
	m_sources.update(source, text->value, version);
	mark_source_available(source);
	schedule_analysis_for_source(source);
}

void LanguageServer::handle_did_change(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = object_at(params, "textDocument");
	const auto* uri = string_at(text_document, "uri");
	if (!uri) return;

	const auto* changes_value = params ? params->get("contentChanges") : nullptr;
	const auto* changes = changes_value ? changes_value->as<JSONArray>() : nullptr;
	if (!changes || changes->size() == 0) return;

	const auto* last_change = changes->at(changes->size() - 1);
	const auto* change = last_change ? last_change->as<JSONObject>() : nullptr;
	const auto* text = string_at(change, "text");
	if (!text) return;

	const auto version = text_document->get_int("version").value_or(0);
	const auto source = source_from_uri(uri->value);
	m_sources.update(source, text->value, version);
	mark_source_available(source);
	schedule_analysis_for_source(source);
}

void LanguageServer::handle_did_close(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = object_at(params, "textDocument");
	const auto* uri = string_at(text_document, "uri");
	if (!uri) return;

	const auto source = source_from_uri(uri->value);
	// Drop the in-memory buffer so reads fall back to disk again.
	m_sources.close(source);
	std::error_code exists_error;
	const bool source_exists = std::filesystem::exists(
		FileSystemSourceProvider::normalize(source.value).value, exists_error);
	if (!source_exists && !exists_error) {
		handle_deleted_source(source);
		return;
	}

	bool standalone_entry = false;
	{
		std::lock_guard lock(m_state_mutex);
		// Only tear down entries that exist solely because the file was open: a standalone
		// orphan entry. The configured entry and files it owns keep their diagnostics so the
		// whole project's diagnostics do not vanish when a tab is closed.
		standalone_entry = m_entry_points.is_known_entry(source)
			&& !m_entry_points.is_configured_entry(source)
			&& !m_entry_points.is_owned_by_configured_entry(source);
		if (standalone_entry) {
			m_entry_points.remove_entry(source);
			m_diagnostic_publisher.clear_entry(source);
			m_references.erase(source);
		}
	}

	if (standalone_entry) {
		std::lock_guard lock(m_analysis_mutex);
		m_pending_analysis_sources.erase(FileSystemSourceProvider::normalize(source.value).value);
		++m_analysis_generation;
	} else {
		// Re-analyse so any unsaved edits that were just discarded are reflected from disk.
		schedule_analysis_for_source(source);
	}
}

void LanguageServer::handle_did_change_watched_files(const JsonRpcMessage& message) {
	// LSP FileChangeType: Created = 1, Changed = 2, Deleted = 3.
	static constexpr int64_t DELETED = 3;
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* changes_value = params ? params->get("changes") : nullptr;
	const auto* changes = changes_value ? changes_value->as<JSONArray>() : nullptr;
	if (!changes) return;

	for (size_t i = 0; i < changes->size(); ++i) {
		const auto* change_value = changes->at(i);
		const auto* change = change_value ? change_value->as<JSONObject>() : nullptr;
		const auto* uri = string_at(change, "uri");
		const auto type = change ? change->get_int("type") : std::nullopt;
		if (!uri || !type) continue;

		const auto source = source_from_uri(uri->value);
		if (*type == DELETED) {
			// An open document remains authoritative even if its backing file disappears.
			// didClose will perform the deletion cleanup once the overlay is released.
			if (!m_sources.load(source).is_error()) {
				schedule_analysis_for_source(source);
				continue;
			}
			handle_deleted_source(source);
		} else {
			mark_source_available(source);
			schedule_analysis_for_source(source);
		}
	}
}
