#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "CompletionProvider.h"
#include "../JSON/ast/JSONValue.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Parameter-name inlay hints for calls of user-defined callables.
 *
 * Like SignatureHelpProvider it owns no analysis state: calls are read from the live
 * buffer and resolved against the completion snapshots, so hints keep working while the
 * buffer is mid-edit. KSP engine commands are not in those snapshots, which is intended:
 * the cksp-tools extension serves their hints from its own database.
 *
 * Every hint carries `data.parameterCount`, so a client can apply a minimum parameter
 * count setting without asking the server again.
 */
class InlayHintProvider {
	const CompletionProvider& m_completion;

public:
	explicit InlayHintProvider(const CompletionProvider& completion)
		: m_completion(completion) {}

	/// Hints for calls whose name starts inside [begin, end) of `text`.
	[[nodiscard]] JSONArray hints(
		const std::vector<SourceId>& preferred_entries,
		std::string_view text,
		size_t begin,
		size_t end,
		const SourceId& source) const;

	/// The label written before an argument: the parameter name, shortened the way the
	/// cksp-tools extension shortens it so hints look the same with and without a server.
	[[nodiscard]] static std::string short_name_of(const std::string& parameter_label);

	/// True when the argument already spells out the parameter, as in <fade(amount, ...)>.
	[[nodiscard]] static bool argument_names_parameter(
		std::string_view argument, const std::string& short_name);
};
