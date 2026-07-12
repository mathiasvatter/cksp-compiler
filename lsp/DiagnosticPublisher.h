//
// Created by Mathias Vatter on 08.07.26.
//

#pragma once

#include <unordered_map>
#include <unordered_set>

#include "../misc/Diagnostic.h"
#include "../cksp/Source/SourceProvider.h"
#include "EntryPointResolver.h"
#include "JsonRpcConnection.h"

class DiagnosticPublisher {
	JsonRpcConnection& m_connection;
	const EntryPointResolver* m_entries = nullptr;
	std::unordered_map<std::string, std::unordered_map<std::string, std::vector<Diagnostic>>> m_diagnostics_by_entry_and_source;
public:
	explicit DiagnosticPublisher(JsonRpcConnection& connection)
		: m_connection(connection) {}

	/// Provides the entry-point ownership information used to suppress diagnostics
	/// that standalone entries produce for files owned by the configured entry.
	void set_entry_resolver(const EntryPointResolver* entries) { m_entries = entries; }

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

	/// Clears a tracked entry without clearing a URI that is not owned by this entry.
	void discard_entry(const SourceId& entry_source);

	/// Clears diagnostics for one source without changing entry ownership.
	void clear_source(const SourceId& source);

private:

	[[nodiscard]] static SourceId diagnostic_source(const Diagnostic& diagnostic, const SourceId& entry_source);
	[[nodiscard]] static std::unique_ptr<JSONObject> make_lsp_diagnostic(const Diagnostic& diagnostic);
	void publish_merged_source(const SourceId& source) const;
	void publish_source(const SourceId& source, const std::vector<Diagnostic>& diagnostics) const;
};
