//
// Created by Mathias Vatter on 07.07.26.
//

#include "LanguageServer.h"

#include "../cksp/Compiler.h"
#include "../misc/DiagnosticSink.h"
#include "version.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {

const JSONObject* object_at(const JSONObject* object, const std::string& key) {
	if (!object) return nullptr;
	return object->get_object(key);
}

const JSONString* string_at(const JSONObject* object, const std::string& key) {
	if (!object) return nullptr;
	return object->get_string(key);
}

int lsp_severity(const DiagnosticSeverity severity) {
	switch (severity) {
		case DiagnosticSeverity::Error: return 1;
		case DiagnosticSeverity::Warning: return 2;
		case DiagnosticSeverity::Information: return 3;
	}
	return 1;
}

size_t lsp_line(const size_t line) {
	if (line == static_cast<size_t>(-1) || line == 0) return 0;
	return line - 1;
}

size_t lsp_character(const size_t column) {
	if (column == static_cast<size_t>(-1) || column == 0) return 0;
	return column - 1;
}

std::unique_ptr<JSONObject> make_position(const size_t line, const size_t character) {
	auto position = std::make_unique<JSONObject>();
	position->add("line", std::make_unique<JSONInt>(static_cast<long long>(line)));
	position->add("character", std::make_unique<JSONInt>(static_cast<long long>(character)));
	return position;
}

std::unique_ptr<JSONObject> make_range(const SourceRange& range) {
	auto lsp_range = std::make_unique<JSONObject>();
	if (!range.is_valid()) {
		lsp_range->add("start", make_position(0, 0));
		lsp_range->add("end", make_position(0, 1));
		return lsp_range;
	}

	const auto start_line = lsp_line(range.start.line);
	const auto start_character = lsp_character(range.start.column);
	auto end_line = lsp_line(range.end.line);
	auto end_character = lsp_character(range.end.column);
	if (end_line < start_line || (end_line == start_line && end_character <= start_character)) {
		end_line = start_line;
		end_character = start_character + 1;
	}

	lsp_range->add("start", make_position(start_line, start_character));
	lsp_range->add("end", make_position(end_line, end_character));
	return lsp_range;
}

std::unique_ptr<JSONObject> make_lsp_diagnostic(const Diagnostic& diagnostic) {
	auto lsp_diagnostic = std::make_unique<JSONObject>();
	lsp_diagnostic->add("range", make_range(diagnostic.range));
	lsp_diagnostic->add("severity", std::make_unique<JSONInt>(lsp_severity(diagnostic.severity)));
	lsp_diagnostic->add("source", std::make_unique<JSONString>("cksp"));
	lsp_diagnostic->add("code", std::make_unique<JSONString>(error_type_to_string(diagnostic.type)));
	lsp_diagnostic->add("message", std::make_unique<JSONString>(diagnostic.message));
	return lsp_diagnostic;
}

bool diagnostic_belongs_to_source(const Diagnostic& diagnostic, const SourceId& source) {
	if (diagnostic.file.empty()) return true;
	return FileSystemSourceProvider::normalize(diagnostic.file) == source;
}

SourceId diagnostic_source_or_entry(const Diagnostic& diagnostic, const SourceId& entry_source) {
	if (diagnostic.file.empty()) return entry_source;
	return FileSystemSourceProvider::normalize(diagnostic.file);
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
	m_entry_sources.insert(entry_source.value);
	m_import_graphs.insert_or_assign(entry_source.value, compiler.import_graph());
	dump_import_graphs();
	publish_analysis_diagnostics(entry_source, diagnostics.diagnostics());
}

void LanguageServer::analyze_entries_for_source(const SourceId& changed_source) {
	const auto entries = entry_sources_for(changed_source);
	for (const auto& entry : entries) {
		analyze_entry(entry);
	}
}

std::vector<SourceId> LanguageServer::entry_sources_for(const SourceId& changed_source) const {
	const auto source = FileSystemSourceProvider::normalize(changed_source.value);
	std::vector<SourceId> entries;

	if (const auto configured_entry = configured_entry_for(source)) {
		entries.push_back(*configured_entry);
		return entries;
	}

	for (const auto& entry_value : m_entry_sources) {
		const SourceId entry(entry_value);
		if (entry == source) {
			entries.push_back(entry);
			continue;
		}

		const auto graph = m_import_graphs.find(entry.value);
		if (graph == m_import_graphs.end()) {
			continue;
		}
		const auto dependents = graph->second.transitive_dependents_of(source);
		if (std::any_of(dependents.begin(), dependents.end(), [&entry](const SourceId& dependent) {
			return dependent == entry;
		})) {
			entries.push_back(entry);
		}
	}

	if (entries.empty()) {
		entries.push_back(source);
	}
	return entries;
}

std::optional<SourceId> LanguageServer::configured_entry_for(const SourceId& changed_source) const {
	if (!m_configured_entry_source) {
		return std::nullopt;
	}
	if (!m_workspace_root) {
		return m_configured_entry_source;
	}

	const auto source_path = std::filesystem::path(FileSystemSourceProvider::normalize(changed_source.value).value);
	const auto root_path = std::filesystem::path(m_workspace_root->value);
	std::error_code error;
	const auto relative = std::filesystem::relative(source_path, root_path, error);
	if (!error && !relative.empty() && *relative.begin() != "..") {
		return m_configured_entry_source;
	}
	return std::nullopt;
}

void LanguageServer::dump_import_graphs() const {
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

void LanguageServer::publish_analysis_diagnostics(const SourceId& entry_source, const std::vector<Diagnostic>& diagnostics) {
	std::unordered_map<std::string, std::vector<Diagnostic>> diagnostics_by_source;
	for (const auto& diagnostic : diagnostics) {
		const auto source = diagnostic_source_or_entry(diagnostic, entry_source);
		diagnostics_by_source[source.value].push_back(diagnostic);
	}

	publish_diagnostics(entry_source, diagnostics_by_source[entry_source.value]);
	std::unordered_set<std::string> current_sources{entry_source.value};
	for (const auto& [source, source_diagnostics] : diagnostics_by_source) {
		current_sources.insert(source);
		if (source != entry_source.value) {
			publish_diagnostics(SourceId(source), source_diagnostics);
		}
	}

	auto& previous_sources = m_published_diagnostic_sources[entry_source.value];
	for (const auto& previous_source : previous_sources) {
		if (!current_sources.contains(previous_source)) {
			publish_diagnostics(SourceId(previous_source), {});
		}
	}
	previous_sources = std::move(current_sources);
}

void LanguageServer::publish_diagnostics(const SourceId& source, const std::vector<Diagnostic>& diagnostics) const {
	auto items = std::make_unique<JSONArray>();
	for (const auto& diagnostic : diagnostics) {
		if (diagnostic_belongs_to_source(diagnostic, source)) {
			items->add(make_lsp_diagnostic(diagnostic));
		}
	}

	JSONObject params;
	params.add("uri", std::make_unique<JSONString>(uri_from_source(source)));
	params.add("diagnostics", std::move(items));
	m_connection.send_notification("textDocument/publishDiagnostics", params);
}

void LanguageServer::handle_initialize(const JsonRpcMessage& message) {
	m_initialized = true;
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	m_workspace_root = source_from_optional_uri_or_path(params, "rootUri", "rootPath");
	if (m_workspace_root) {
		m_workspace_root = FileSystemSourceProvider::normalize(m_workspace_root->value);
	}
	m_configured_entry_source = resolve_configured_entry(params, m_workspace_root);

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
	m_entry_sources.erase(source.value);
	m_import_graphs.erase(source.value);
	if (const auto published = m_published_diagnostic_sources.find(source.value);
		published != m_published_diagnostic_sources.end()) {
		for (const auto& published_source : published->second) {
			publish_diagnostics(SourceId(published_source), {});
		}
		m_published_diagnostic_sources.erase(published);
	}
	publish_diagnostics(source, {});
}
