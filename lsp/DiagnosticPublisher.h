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
	/// For every source last published with diagnostics, the entries its attribution was
	/// decided against. A change of those - an entry analysed for the first time, one that
	/// dropped or gained the import, one deleted - changes which diagnostics name their
	/// entries, even when the source's own diagnostics stay the same.
	mutable std::unordered_map<std::string, std::vector<SourceId>> m_published_reaching;
public:
	explicit DiagnosticPublisher(JsonRpcConnection& connection)
		: m_connection(connection) {}

	/// Provides the entry-point ownership information used to suppress diagnostics
	/// that standalone entries produce for files owned by configured entries.
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

	/**
	 * The diagnostics currently published for one source file.
	 *
	 * A code action must not reconstruct this from the diagnostics a request carries: a client
	 * sends only those intersecting the requested range, and which range that is depends on how
	 * the action was triggered - the whole document on save, the selection for a manually
	 * invoked one. Reading what was published keeps a whole-file action whole either way.
	 */
	[[nodiscard]] std::vector<Diagnostic> diagnostics_for(const SourceId& source) const;

	/// The LSP `{targetUri, range, newText}` an edit describes. Insertions carry an empty range
	/// at the insertion point, replacements the range they overwrite.
	[[nodiscard]] static std::unique_ptr<JSONObject> make_lsp_edit_data(
		const Diagnostic::DiagnosticFix::Edit& edit
	);

private:

	/// One published diagnostic plus the entries whose analysis reported it.
	struct MergedDiagnostic {
		Diagnostic diagnostic;
		std::vector<SourceId> reported_by;
	};

	[[nodiscard]] static SourceId diagnostic_source(const Diagnostic& diagnostic, const SourceId& entry_source);
	[[nodiscard]] static bool same_published_diagnostic(
		const Diagnostic& left,
		const Diagnostic& right
	);
	[[nodiscard]] static std::unique_ptr<JSONObject> make_lsp_diagnostic(
		const Diagnostic& diagnostic,
		const std::vector<SourceId>& reported_by = {}
	);
	[[nodiscard]] static std::unique_ptr<JSONObject> make_lsp_fix_data(const Diagnostic::DiagnosticFix& fix);
	[[nodiscard]] std::vector<MergedDiagnostic> merged_diagnostics(const SourceId& source) const;
	void publish_merged_source(const SourceId& source) const;
	/// The configured entries a source's diagnostics are attributed against: empty when
	/// naming entries cannot say anything for it.
	[[nodiscard]] std::vector<SourceId> attribution_entries(const SourceId& source) const;
	/// Adds the published sources whose attribution entries changed since their last publish.
	void add_stale_attributions(std::unordered_set<std::string>& affected_sources) const;
	void publish_source(const SourceId& source, const std::vector<MergedDiagnostic>& diagnostics) const;
};
