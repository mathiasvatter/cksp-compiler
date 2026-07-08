//
// Created by Mathias Vatter on 07.07.26.
//

#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../cksp/Source/ImportGraph.h"
#include "../cksp/Source/SourceProvider.h"
#include "JsonRpcConnection.h"
#include "../misc/Diagnostic.h"

class LanguageServer {
	JsonRpcConnection& m_connection;
	FileSystemSourceProvider m_file_sources;
	OverlaySourceProvider m_sources;
	std::unordered_set<std::string> m_entry_sources;
	std::unordered_map<std::string, ImportGraph> m_import_graphs;
	std::unordered_map<std::string, std::unordered_set<std::string>> m_published_diagnostic_sources;
	std::optional<SourceId> m_configured_entry_source;
	std::optional<SourceId> m_workspace_root;

	bool m_initialized = false;
	bool m_shutdown_requested = false;
	bool m_exit_requested = false;

	void analyze_entry(const SourceId& entry_source);
	void analyze_entries_for_source(const SourceId& changed_source);
	[[nodiscard]] std::vector<SourceId> entry_sources_for(const SourceId& changed_source) const;
	[[nodiscard]] std::optional<SourceId> configured_entry_for(const SourceId& changed_source) const;
	void dump_import_graphs() const;
	void publish_analysis_diagnostics(const SourceId& entry_source, const std::vector<Diagnostic>& diagnostics);
	void publish_diagnostics(const SourceId& source, const std::vector<Diagnostic>& diagnostics) const;

public:
	explicit LanguageServer(JsonRpcConnection& connection)
		: m_connection(connection),
		  m_sources(m_file_sources) {}

	int run();

	[[nodiscard]] SourceProvider& source_provider() {
		return m_sources;
	}

	[[nodiscard]] OverlaySourceProvider& overlay_sources() {
		return m_sources;
	}

	void handle_message(const JsonRpcMessage& message);
	void handle_request(const JsonRpcMessage& message);
	void handle_notification(const JsonRpcMessage& message);

	void handle_initialize(const JsonRpcMessage& message);
	void handle_shutdown(const JsonRpcMessage& message);

	void handle_did_open(const JsonRpcMessage& message);
	void handle_did_change(const JsonRpcMessage& message);
	void handle_did_close(const JsonRpcMessage& message);
};
