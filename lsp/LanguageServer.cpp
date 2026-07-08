//
// Created by Mathias Vatter on 07.07.26.
//

#include "LanguageServer.h"

#include "../cksp/Compiler.h"
#include "../misc/DiagnosticSink.h"
#include "version.h"

#include <filesystem>

namespace {

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

int LanguageServer::run() {
	while (auto message = m_connection.read_message()) {
		handle_message(*message);
		if (m_exit_requested) {
			break;
		}
	}
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

void LanguageServer::analyze_entry(const SourceId& entry_source) {
	CollectingDiagnosticSink diagnostics;
	auto config = std::make_unique<CompilerConfig>();
	Compiler compiler(std::move(config), m_sources);
	compiler.analyze(entry_source, diagnostics);
	m_entry_points.register_analysis(entry_source, compiler.import_graph());
	m_diagnostic_publisher.publish(entry_source, diagnostics.diagnostics());
}

void LanguageServer::analyze_entries_for_source(const SourceId& changed_source) {
	for (const auto& entry : m_entry_points.affected_entries(changed_source)) {
		analyze_entry(entry);
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
	m_entry_points.set_workspace_root(m_workspace_root);
	m_entry_points.set_configured_entry(m_configured_entry_source);

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
	analyze_entries_for_source(source);
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
	analyze_entries_for_source(source);
}

void LanguageServer::handle_did_close(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = object_at(params, "textDocument");
	const auto* uri = string_at(text_document, "uri");
	if (!uri) return;

	const auto source = source_from_uri(uri->value);
	m_sources.close(source);
	m_entry_points.remove_entry(source);
	m_diagnostic_publisher.clear_entry(source);
}
