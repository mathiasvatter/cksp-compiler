//
// Created by Mathias Vatter on 07.07.26.
//

#include "LanguageServer.h"

#include "../cksp/Compiler.h"
#include "../misc/DiagnosticSink.h"
#include "version.h"

#include <chrono>
#include <filesystem>
#include <unordered_set>

namespace {
constexpr auto ANALYSIS_DEBOUNCE = std::chrono::milliseconds(120);

const JSONObject* object_at(const JSONObject* object, const std::string& key) {
	if (!object) return nullptr;
	return object->get_object(key);
}

const JSONString* string_at(const JSONObject* object, const std::string& key) {
	if (!object) return nullptr;
	return object->get_string(key);
}

std::optional<SourceId> source_from_optional_uri_or_path(const JSONObject* object, const std::string& uri_key, const std::string& path_key) {
	if (const auto* uri = string_at(object, uri_key)) {
		return source_from_uri(uri->value);
	}
	if (const auto* path = string_at(object, path_key)) {
		return FileSystemSourceProvider::normalize(path->value);
	}
	return std::nullopt;
}

std::optional<SourceId> resolve_configured_entry(const JSONObject* initialize_params, const std::optional<SourceId>& workspace_root) {
	const auto* options = object_at(initialize_params, "initializationOptions");
	auto entry = source_from_optional_uri_or_path(options, "mainFileUri", "mainFilePath");
	if (!entry) return std::nullopt;

	std::filesystem::path path(entry->value);
	if (path.is_relative() && workspace_root) {
		path = std::filesystem::path(workspace_root->value) / path;
	}
	return FileSystemSourceProvider::normalize(path.string());
}
}

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

void LanguageServer::analyze_entry(const SourceId& entry_source, const uint64_t generation) {
	CollectingDiagnosticSink diagnostics;
	auto config = std::make_unique<CompilerConfig>();
	config->lsp = true;
	Compiler compiler(std::move(config), m_sources);
	compiler.analyze(entry_source, diagnostics);
	if (!is_analysis_current(generation)) {
		return;
	}
	std::lock_guard lock(m_state_mutex);
	m_entry_points.register_analysis(entry_source, compiler.import_graph());
	m_diagnostic_publisher.publish(entry_source, diagnostics.diagnostics());
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
				}
			}
		}

		if (entries.empty()) {
			return;
		}

		for (const auto& entry : entries) {
			if (!is_analysis_current(generation)) {
				return;
			}
			analyze_entry(entry, generation);
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

	JSONObject sync;
	sync.add("openClose", std::make_unique<JSONBool>(true));
	sync.add("change", std::make_unique<JSONInt>(1));

	JSONObject capabilities;
	capabilities.add("textDocumentSync", std::make_unique<JSONObject>(sync));

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

void LanguageServer::handle_did_open(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = object_at(params, "textDocument");
	const auto* uri = string_at(text_document, "uri");
	const auto* text = string_at(text_document, "text");
	if (!uri || !text) return;

	const auto version = text_document->get_int("version").value_or(0);
	const auto source = source_from_uri(uri->value);
	m_sources.update(source, text->value, version);
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
	schedule_analysis_for_source(source);
}

void LanguageServer::handle_did_close(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = object_at(params, "textDocument");
	const auto* uri = string_at(text_document, "uri");
	if (!uri) return;

	const auto source = source_from_uri(uri->value);
	m_sources.close(source);
	{
		std::lock_guard lock(m_analysis_mutex);
		m_pending_analysis_sources.erase(FileSystemSourceProvider::normalize(source.value).value);
		++m_analysis_generation;
	}
	{
		std::lock_guard lock(m_state_mutex);
		m_entry_points.remove_entry(source);
		m_diagnostic_publisher.clear_entry(source);
	}
}
