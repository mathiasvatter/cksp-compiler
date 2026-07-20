#pragma once

#include <optional>
#include <string>
#include <vector>

#include "ReferenceProvider.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Implements the rename rules on top of the reference snapshots.
 *
 * ReferenceProvider answers where a symbol is declared and used; this class decides whether
 * a position is renameable, whether a new name is acceptable, and verifies against the live
 * buffers that every planned edit still matches the analyzed text before any edit is handed
 * out. Translating the results to LSP JSON stays in the LanguageServer.
 */
class RenameProvider {
public:
	RenameProvider(ReferenceProvider& references, SourceProvider& sources)
		: m_references(references), m_sources(sources) {}

	/// The exact symbol range at the request position: the clicked reference or, when the
	/// cursor sits on the declaration itself, the declared name. Nothing when the position
	/// is not renameable through the link, e.g. inside a function header's parameter list.
	[[nodiscard]] static std::optional<SourceRange> renameable_range(
		const ReferenceLink& target,
		const SourceId& source,
		size_t line,
		size_t character);

	struct RenameResult {
		/// Every location whose text is replaced by the new name, including the declaration.
		std::vector<ReferenceLocation> edits;
		/// The reason the rename is refused; empty on success.
		std::string error;
		[[nodiscard]] bool ok() const { return error.empty(); }
	};

	/// Collects and validates every edit renaming the target to new_name. The whole rename
	/// is refused when the new name is not an identifier or any location is out of sync
	/// with the live buffers - a partial or misplaced rename would corrupt the code.
	[[nodiscard]] RenameResult rename(const ReferenceLink& target, const std::string& new_name) const;

private:
	ReferenceProvider& m_references;
	SourceProvider& m_sources;

	/// The source text covered by a single-line range, read through the overlay so it
	/// reflects the current editor buffers. Nothing when the range no longer fits the text.
	[[nodiscard]] std::optional<std::string> text_at(const std::string& file, const SourceRange& range) const;

	/// A name a rename may introduce: a plain identifier, a qualified name (defines like
	/// ui.colors.WHITE) or a macro parameter (#var#).
	[[nodiscard]] static bool is_valid_name(const std::string& name);
};
