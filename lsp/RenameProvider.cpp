#include "RenameProvider.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

std::optional<SourceRange> RenameProvider::renameable_range(
	const ReferenceLink& target,
	const SourceId& source,
	const size_t line,
	const size_t character) {
	if (target.ref_file == source.value
		&& ReferenceIndex::covers(target.ref_range, line, character)) {
		return target.ref_range;
	}
	if (target.def_file == source.value
		&& ReferenceIndex::covers(target.def_name_range, line, character)) {
		return target.def_name_range;
	}
	return std::nullopt;
}

RenameProvider::RenameResult RenameProvider::rename(
	const ReferenceLink& target,
	const std::string& new_name) const {
	if (!is_valid_name(new_name)) {
		return {{}, "'" + new_name + "' is not a valid identifier."};
	}

	auto locations = m_references.references_to(target, true);

	// Every edit must still match the analyzed text: the identifier read from the live
	// buffers has to be identical at the declaration and at every reference. A mismatch
	// means the analysis is stale (or a location is a rewritten alias such as a raw-array
	// reference) - renaming would corrupt the code.
	const auto expected = text_at(target.def_file, target.def_name_range);
	if (!expected) {
		return {{}, "The declaration is out of sync with the last analysis."
			" Retry once the analysis has caught up."};
	}
	for (const auto& location : locations) {
		const auto actual = text_at(location.file, location.range);
		if (!actual || *actual != *expected) {
			return {{}, "A reference in " + std::filesystem::path(location.file).filename().string()
				+ " is out of sync with the last analysis. Retry once the analysis has caught up."};
		}
	}

	return {std::move(locations), {}};
}

std::optional<std::string> RenameProvider::text_at(const std::string& file, const SourceRange& range) const {
	if (!range.is_valid() || range.start.line != range.end.line || range.start.column < 1) {
		return std::nullopt;
	}
	auto loaded = m_sources.load(SourceId(file));
	if (loaded.is_error()) return std::nullopt;
	const auto& text = *loaded.unwrap().text;

	size_t offset = 0;
	for (size_t line = 1; line < range.start.line; ++line) {
		offset = text.find('\n', offset);
		if (offset == std::string::npos) return std::nullopt;
		++offset;
	}
	const size_t line_end = std::min(text.find('\n', offset), text.size());
	const size_t start = offset + (range.start.column - 1);
	const size_t end = offset + (range.end.column - 1);
	if (start > end || end > line_end) return std::nullopt;
	return text.substr(start, end - start);
}

bool RenameProvider::is_valid_name(const std::string& name) {
	if (name.empty()) return false;
	if (std::isdigit(static_cast<unsigned char>(name.front()))) return false;
	for (const char c : name) {
		const bool valid = std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '#';
		if (!valid) return false;
	}
	return true;
}
