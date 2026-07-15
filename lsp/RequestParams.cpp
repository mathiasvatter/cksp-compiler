#include "RequestParams.h"

#include <filesystem>

namespace lsp {

const JSONObject* object_at(const JSONObject* object, const std::string& key) {
	if (!object) return nullptr;
	return object->get_object(key);
}

const JSONString* string_at(const JSONObject* object, const std::string& key) {
	if (!object) return nullptr;
	return object->get_string(key);
}

std::optional<TextDocumentPosition> position_params(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = object_at(params, "textDocument");
	const auto* uri = string_at(text_document, "uri");
	const auto* position = object_at(params, "position");
	if (!uri || !position) return std::nullopt;

	return TextDocumentPosition{
		source_from_uri(uri->value),
		static_cast<size_t>(position->get_int("line").value_or(0)),
		static_cast<size_t>(position->get_int("character").value_or(0))};
}

std::optional<SourceId> source_from_optional_uri_or_path(
	const JSONObject* object,
	const std::string& uri_key,
	const std::string& path_key) {
	if (const auto* uri = string_at(object, uri_key)) {
		return source_from_uri(uri->value);
	}
	if (const auto* path = string_at(object, path_key)) {
		return FileSystemSourceProvider::normalize(path->value);
	}
	return std::nullopt;
}

std::optional<SourceId> resolve_configured_entry(
	const JSONObject* initialize_params,
	const std::optional<SourceId>& workspace_root) {
	const auto* options = object_at(initialize_params, "initializationOptions");
	auto entry = source_from_optional_uri_or_path(options, "mainFileUri", "mainFilePath");
	if (!entry) return std::nullopt;

	std::filesystem::path path(entry->value);
	if (path.is_relative() && workspace_root) {
		path = std::filesystem::path(workspace_root->value) / path;
	}
	return FileSystemSourceProvider::normalize(path.string());
}

}
