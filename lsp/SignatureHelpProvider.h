#pragma once

#include <memory>
#include <vector>

#include "CallSiteScanner.h"
#include "CompletionProvider.h"
#include "../JSON/ast/JSONValue.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Renders LSP SignatureHelp from the callable metadata already owned by the
 * completion snapshots. It deliberately owns no analysis state of its own.
 */
class SignatureHelpProvider {
	const CompletionProvider& m_completion;

public:
	explicit SignatureHelpProvider(const CompletionProvider& completion)
		: m_completion(completion) {}

	/// Returns null when the source-level call resolves to no indexed callable.
	[[nodiscard]] std::unique_ptr<JSONObject> help(
		const std::vector<SourceId>& preferred_entries,
		const lsp::CallSiteQuery& query,
		const SourceId& source,
		size_t line,
		size_t character) const;
};
