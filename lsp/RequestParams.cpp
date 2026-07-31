#include "RequestParams.h"

#include <filesystem>
#include <unordered_set>

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

std::vector<SourceId> resolve_configured_entries(
	const JSONObject* initialize_params,
	const std::optional<SourceId>& workspace_root) {
	const auto* options = object_at(initialize_params, "initializationOptions");
	std::vector<SourceId> entries;
	std::unordered_set<std::string> seen;
	const auto add_entry = [&](SourceId entry) {
		std::filesystem::path path(entry.value);
		if (path.is_relative() && workspace_root) {
			path = std::filesystem::path(workspace_root->value) / path;
		}
		entry = FileSystemSourceProvider::normalize(path.string());
		if (seen.insert(entry.value).second) {
			entries.push_back(std::move(entry));
		}
	};

	if (auto main_entry = source_from_optional_uri_or_path(options, "mainFileUri", "mainFilePath")) {
		add_entry(std::move(*main_entry));
	}

	const auto* entry_points_value = options ? options->get("entryPoints") : nullptr;
	const auto* entry_points = entry_points_value ? entry_points_value->as<JSONArray>() : nullptr;
	if (!entry_points) {
		return entries;
	}

	for (size_t index = 0; index < entry_points->size(); ++index) {
		const auto* entry = entry_points->at(index);
		const auto* entry_path = entry ? entry->as<JSONString>() : nullptr;
		if (entry_path && !entry_path->value.empty()) {
			add_entry(SourceId(entry_path->value));
		}
	}
	return entries;
}

}
