#pragma once

#include <optional>
#include <string>

#include "JsonRpcMessage.h"
#include "../cksp/Source/SourceProvider.h"

/**
 * Typed accessors over LSP request parameters.
 *
 * The request handlers in LanguageServer work with these views instead of walking the raw
 * JSON; every accessor tolerates missing or malformed members and returns nothing in that
 * case, so the handlers only deal with well-formed input.
 */
namespace lsp {

/// The textDocument/position pair shared by all position-based requests.
struct TextDocumentPosition {
	SourceId source;
	size_t line = 0;
	size_t character = 0;
};

/// Null-safe member lookup: the nested object at key, or nothing.
[[nodiscard]] const JSONObject* object_at(const JSONObject* object, const std::string& key);

/// Null-safe member lookup: the string at key, or nothing.
[[nodiscard]] const JSONString* string_at(const JSONObject* object, const std::string& key);

[[nodiscard]] std::optional<TextDocumentPosition> position_params(const JsonRpcMessage& message);

/// Reads a source location given either as URI or as filesystem path.
[[nodiscard]] std::optional<SourceId> source_from_optional_uri_or_path(
	const JSONObject* object,
	const std::string& uri_key,
	const std::string& path_key);

/// The configured main file from the initialize request, resolved against the workspace root.
[[nodiscard]] std::optional<SourceId> resolve_configured_entry(
	const JSONObject* initialize_params,
	const std::optional<SourceId>& workspace_root);

}
