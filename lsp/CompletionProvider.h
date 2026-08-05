#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../JSON/ast/JSONValue.h"
#include "../cksp/Source/CompletionIndex.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Owns the queryable completion snapshots, one per analyzed entry point.
 *
 * Deliberately unlike ReferenceProvider: navigation must not hand out ranges
 * that no longer match the buffer, so it validates its snapshot against the
 * current source text. Completion is asked precisely when the buffer does not
 * match — the user is typing, and the half-written line usually fails to parse.
 * Names stay valid across edits and no ranges are returned, so the last
 * successful snapshot is served unconditionally. Without that, completion would
 * go silent exactly when it is needed.
 */
class CompletionProvider {
public:
	void publish(const SourceId& entry, CompletionIndex index, bool successful);
	void erase(const SourceId& entry);

	/// LSP completion items for a typed qualifier chain at a cursor position. Entries
	/// are consulted in the order given (the caller passes the entry points owning the
	/// document); the position resolves shortened qualifiers against the block they are
	/// written in.
	[[nodiscard]] JSONArray items(
		const std::vector<SourceId>& preferred_entries,
		const std::vector<std::string>& chain,
		const SourceId& source,
		size_t line,
		size_t character) const;

private:
	/// Members behind items(), kept separate so the lookup stays testable on its own.
	[[nodiscard]] std::vector<CompletionMember> members(
		const std::vector<SourceId>& preferred_entries,
		const std::vector<std::string>& chain,
		const SourceId& source,
		size_t line,
		size_t character) const;

	/// The source-construct word shown greyed after the label, matching the
	/// `description` the cksp-tools extension puts on its own completion items.
	[[nodiscard]] static std::string category_of(CompletionKind kind);
	[[nodiscard]] static std::unique_ptr<JSONObject> make_item(const CompletionMember& member);

	struct State {
		/// Last complete index. Survives failing analyses on purpose.
		std::shared_ptr<const CompletionIndex> last_successful;
		/// Most recent index, used only until a successful one exists.
		std::shared_ptr<const CompletionIndex> current;
	};

	std::unordered_map<std::string, State> m_states;
	mutable std::mutex m_mutex;
};
