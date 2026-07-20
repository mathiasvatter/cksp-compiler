#include "ReferenceProvider.h"

#include <algorithm>
#include <unordered_set>

void ReferenceProvider::publish(
	const SourceId& entry,
	ReferenceIndex index,
	const bool successful,
	SourceContents successful_sources) {
	const auto normalized_entry = FileSystemSourceProvider::normalize(entry.value);
	auto current = std::make_shared<const ReferenceIndex>(std::move(index));

	std::lock_guard lock(m_mutex);
	auto& state = m_states[normalized_entry.value];
	state.current = std::move(current);
	if (successful) {
		state.last_successful = state.current;
		state.last_successful_sources = std::move(successful_sources);
	}
}

void ReferenceProvider::erase(const SourceId& entry) {
	std::lock_guard lock(m_mutex);
	m_states.erase(FileSystemSourceProvider::normalize(entry.value).value);
}

bool ReferenceProvider::has_successful_snapshot(const SourceId& entry) const {
	std::lock_guard lock(m_mutex);
	const auto state = m_states.find(FileSystemSourceProvider::normalize(entry.value).value);
	return state != m_states.end() && state->second.last_successful;
}

bool ReferenceProvider::source_matches_snapshot(const SourceContents& snapshot, const std::string& source) {
	const auto normalized = FileSystemSourceProvider::normalize(source).value;
	const auto expected = snapshot.find(normalized);
	if (expected == snapshot.end()) return false;

	auto current = m_sources.load(SourceId(normalized));
	if (current.is_error()) return false;
	return *current.unwrap().text == *expected->second;
}

std::optional<ReferenceLink> ReferenceProvider::resolve_from_state(
	const State& state,
	const SourceId& source,
	const size_t line,
	const size_t character) {
	if (state.current) {
		if (auto link = state.current->resolve_target(source.value, line, character)) return link;
	}

	if (!state.last_successful || state.last_successful == state.current
		|| !source_matches_snapshot(state.last_successful_sources, source.value)) {
		return std::nullopt;
	}
	auto link = state.last_successful->resolve_target(source.value, line, character);
	if (!link || !source_matches_snapshot(state.last_successful_sources, link->def_file)) {
		return std::nullopt;
	}
	return link;
}

std::optional<ReferenceLink> ReferenceProvider::resolve_target(
	const std::vector<SourceId>& preferred_entries,
	const SourceId& source,
	const size_t line,
	const size_t character) {
	const auto normalized_source = FileSystemSourceProvider::normalize(source.value);
	std::lock_guard lock(m_mutex);

	for (const auto& entry : preferred_entries) {
		const auto state = m_states.find(FileSystemSourceProvider::normalize(entry.value).value);
		if (state == m_states.end()) continue;
		if (auto target = resolve_from_state(state->second, normalized_source, line, character)) return target;
	}
	for (const auto& [_, state] : m_states) {
		if (auto target = resolve_from_state(state, normalized_source, line, character)) return target;
	}
	return std::nullopt;
}

std::vector<ReferenceLink> ReferenceProvider::references_from_state(
	const State& state,
	const ReferenceLink& target) {
	std::vector<ReferenceLink> references;
	if (state.current) references = state.current->references_to(target);

	if (!state.last_successful || state.last_successful == state.current
		|| !source_matches_snapshot(state.last_successful_sources, target.def_file)) {
		return references;
	}
	for (auto& link : state.last_successful->references_to(target)) {
		// The partial current analysis wins at positions it managed to resolve, even when
		// the new target differs from the last successful analysis.
		if (state.current && state.current->contains_reference(link.ref_file, link.ref_range)) continue;
		if (!source_matches_snapshot(state.last_successful_sources, link.ref_file)) continue;
		references.push_back(std::move(link));
	}
	return references;
}

std::vector<ReferenceLocation> ReferenceProvider::references_to(
	const ReferenceLink& target,
	const bool include_declaration) {
	std::vector<ReferenceLocation> locations;
	std::unordered_set<std::string> seen;
	const auto add = [&locations, &seen](const std::string& file, const SourceRange& range) {
		auto key = file + "@" + range.to_string();
		if (seen.insert(std::move(key)).second) locations.push_back({file, range});
	};

	std::lock_guard lock(m_mutex);
	for (const auto& [_, state] : m_states) {
		for (const auto& reference : references_from_state(state, target)) {
			add(reference.ref_file, reference.ref_range);
		}
	}
	// the declaration is listed (and rename-edited) at its name, not the whole header range
	if (include_declaration) add(target.def_file, target.def_name_range);

	std::ranges::sort(locations, [](const ReferenceLocation& a, const ReferenceLocation& b) {
		if (a.file != b.file) return a.file < b.file;
		if (a.range.start.line != b.range.start.line) return a.range.start.line < b.range.start.line;
		if (a.range.start.column != b.range.start.column) return a.range.start.column < b.range.start.column;
		if (a.range.end.line != b.range.end.line) return a.range.end.line < b.range.end.line;
		return a.range.end.column < b.range.end.column;
	});
	return locations;
}
