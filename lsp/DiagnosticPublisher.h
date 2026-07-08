//
// Created by Mathias Vatter on 08.07.26.
//

#pragma once

#include <unordered_set>

#include "../misc/Diagnostic.h"
#include "../cksp/Source/SourceProvider.h"
#include "JsonRpcConnection.h"

class DiagnosticPublisher {
	JsonRpcConnection& m_connection;
	/// Maps an entry source to all source files for which the last analysis of that entry published diagnostics.
	std::unordered_map<std::string, std::unordered_set<std::string>> m_published_sources_by_entry;
public:
	explicit DiagnosticPublisher(JsonRpcConnection& connection)
		: m_connection(connection) {}

	/**
	 * Publishes all diagnostics produced while analyzing one entry source.
	 *
	 * Diagnostics are grouped by their actual source file. Sources that had
	 * diagnostics during the previous analysis but no longer have any are
	 * cleared automatically.
	 */
	void publish(const SourceId& entry_source, const std::vector<Diagnostic>& diagnostics);

	/// Clears every diagnostic previously published for this entry source.
	void clear_entry(const SourceId& entry_source);

	/// Clears diagnostics for one source without changing entry ownership.
	void clear_source(const SourceId& source) const;

private:

	[[nodiscard]] static SourceId diagnostic_source(const Diagnostic& diagnostic, const SourceId& entry_source);
	[[nodiscard]] static std::unique_ptr<JSONObject> make_lsp_diagnostic(const Diagnostic& diagnostic);
	void publish_source(const SourceId& source, const std::vector<Diagnostic>& diagnostics) const;
};