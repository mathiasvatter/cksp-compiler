//
// Created by Mathias Vatter on 07.07.26.
//

#pragma once
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "DiagnosticPublisher.h"
#include "EntryPointResolver.h"
#include "ReferenceProvider.h"
#include "RenameProvider.h"
#include "../cksp/Source/SourceProvider.h"
#include "JsonRpcConnection.h"

class LanguageServer {
	/// Editing pauses shorter than this are coalesced into one analysis batch.
	static constexpr auto ANALYSIS_DEBOUNCE = std::chrono::milliseconds(120);

	JsonRpcConnection& m_connection;
	DiagnosticPublisher m_diagnostic_publisher;
	FileSystemSourceProvider m_file_sources;
	OverlaySourceProvider m_sources;
	ReferenceProvider m_references;
	RenameProvider m_rename;
	std::optional<SourceId> m_configured_entry_source;
	std::optional<SourceId> m_workspace_root;
	EntryPointResolver m_entry_points;
	std::unordered_set<std::string> m_deleted_sources;
	mutable std::mutex m_state_mutex;

	mutable std::mutex m_analysis_mutex;
	std::condition_variable m_analysis_cv;
	std::unordered_set<std::string> m_pending_analysis_sources;
	std::thread m_analysis_worker;
	uint64_t m_analysis_generation = 0;
	bool m_stop_analysis_worker = false;

	bool m_initialized = false;
	bool m_shutdown_requested = false;
	bool m_exit_requested = false;

	void schedule_analysis_for_source(const SourceId& changed_source);
	void analysis_worker_loop();
	void stop_analysis_worker();
	void analyze_entry(const SourceId& entry_source);
	void analyze_entries_for_sources(const std::vector<SourceId>& changed_sources, uint64_t generation);
	void mark_source_available(const SourceId& source);
	void handle_deleted_source(const SourceId& source);
	[[nodiscard]] bool is_analysis_current(uint64_t generation) const;
	[[nodiscard]] std::optional<ReferenceLink> resolve_navigation_target(
		const JsonRpcMessage& message);
	[[nodiscard]] std::optional<ReferenceLink> resolve_target_at(
		const SourceId& source, size_t line, size_t character);

public:
	explicit LanguageServer(JsonRpcConnection& connection)
		: m_connection(connection), m_diagnostic_publisher(connection), m_sources(m_file_sources),
		  m_references(m_sources), m_rename(m_references, m_sources) {
		m_diagnostic_publisher.set_entry_resolver(&m_entry_points);
		m_analysis_worker = std::thread(&LanguageServer::analysis_worker_loop, this);
	}
	~LanguageServer();

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
	void handle_definition(const JsonRpcMessage& message);
	void handle_references(const JsonRpcMessage& message);
	void handle_prepare_rename(const JsonRpcMessage& message);
	void handle_rename(const JsonRpcMessage& message);
	void handle_document_highlight(const JsonRpcMessage& message);

	void handle_did_open(const JsonRpcMessage& message);
	void handle_did_change(const JsonRpcMessage& message);
	void handle_did_close(const JsonRpcMessage& message);
	void handle_did_change_watched_files(const JsonRpcMessage& message);
};
