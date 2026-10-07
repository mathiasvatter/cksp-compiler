#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "CompletionProvider.h"
#include "../JSON/ast/JSONValue.h"
#include "../cksp/Source/ReferenceIndex.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Inlay hints: parameter names for calls of user-defined callables, and the inferred type
 * of declarations written without one.
 *
 * Like SignatureHelpProvider it owns no analysis state: calls are read from the live
 * buffer and resolved against the completion snapshots, so parameter hints keep working
 * while the buffer is mid-edit. KSP engine commands are not in those snapshots, which is
 * intended: the cksp-tools extension serves their hints from its own database. Type hints
 * are positions, so the caller passes only those of a snapshot matching the buffer.
 *
 * Every parameter hint carries `data.parameterCount`, so a client can apply a minimum
 * parameter count setting without asking the server again.
 */
class InlayHintProvider {
	const CompletionProvider& m_completion;

public:
	explicit InlayHintProvider(const CompletionProvider& completion)
		: m_completion(completion) {}

	/// Hints for calls whose name starts, and declarations that end, inside [begin, end).
	[[nodiscard]] JSONArray hints(
		const std::vector<SourceId>& preferred_entries,
		std::string_view text,
		size_t begin,
		size_t end,
		const SourceId& source,
		const std::vector<TypeHint>& type_hints) const;

	/// The label written before an argument: the parameter name, shortened the way the
	/// cksp-tools extension shortens it so hints look the same with and without a server.
	[[nodiscard]] static std::string short_name_of(const std::string& parameter_label);

	/// True when the argument already spells out the parameter, as in <fade(amount, ...)>.
	[[nodiscard]] static bool argument_names_parameter(
		std::string_view argument, const std::string& short_name);
};
