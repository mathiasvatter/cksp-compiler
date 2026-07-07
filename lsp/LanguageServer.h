//
// Created by Mathias Vatter on 07.07.26.
//

#pragma once
#include <optional>
#include <string>

#include "../cksp/Source/SourceProvider.h"
#include "JsonRpcConnection.h"

class LanguageServer {
	JsonRpcConnection& m_connection;
	FileSystemSourceProvider m_file_sources;
	OverlaySourceProvider m_sources;

	bool m_initialized = false;
	bool m_shutdown_requested = false;

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
