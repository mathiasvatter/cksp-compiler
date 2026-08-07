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

std::vector<DefinitionLink> ReferenceProvider::resolve_definition_from_state(
	const State& state,
	const SourceId& source,
	const size_t line,
	const size_t character) {
	if (state.current) {
		if (auto links = state.current->resolve_definition(source.value, line, character);
			!links.empty()) {
			return links;
		}
	}

	if (!state.last_successful || state.last_successful == state.current
		|| !source_matches_snapshot(state.last_successful_sources, source.value)) {
		return {};
	}
	auto links = state.last_successful->resolve_definition(source.value, line, character);
	// A stale snapshot may point into a file that has moved on since; drop those targets
	// rather than the whole answer.
	std::erase_if(links, [&](const DefinitionLink& link) {
		return !source_matches_snapshot(state.last_successful_sources, link.def_file);
	});
	return links;
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

std::vector<DefinitionLink> ReferenceProvider::resolve_definition(
	const std::vector<SourceId>& preferred_entries,
	const SourceId& source,
	const size_t line,
	const size_t character) {
	const auto normalized_source = FileSystemSourceProvider::normalize(source.value);
	std::lock_guard lock(m_mutex);

	for (const auto& entry : preferred_entries) {
		const auto state = m_states.find(FileSystemSourceProvider::normalize(entry.value).value);
		if (state == m_states.end()) continue;
		if (auto targets = resolve_definition_from_state(
			state->second, normalized_source, line, character); !targets.empty()) {
			return targets;
		}
	}
	for (const auto& [_, state] : m_states) {
		if (auto targets = resolve_definition_from_state(
			state, normalized_source, line, character); !targets.empty()) {
			return targets;
		}
	}
	return {};
}

std::vector<DefinitionLink> ReferenceProvider::document_links_from_state(
	const State& state,
	const SourceId& source) {
	std::vector<DefinitionLink> links;
	if (state.current) links = state.current->definition_links_in(source.value);

	if (!state.last_successful || state.last_successful == state.current
		|| !source_matches_snapshot(state.last_successful_sources, source.value)) {
		return links;
	}
	for (auto& link : state.last_successful->definition_links_in(source.value)) {
		if (state.current
			&& state.current->contains_definition_link(link.ref_file, link.ref_range)) {
			continue;
		}
		if (!source_matches_snapshot(state.last_successful_sources, link.def_file)) continue;
		links.push_back(std::move(link));
	}
	return links;
}

std::vector<DefinitionLink> ReferenceProvider::document_links(
	const std::vector<SourceId>& preferred_entries,
	const SourceId& source) {
	const auto normalized_source = FileSystemSourceProvider::normalize(source.value);
	std::vector<DefinitionLink> links;
	std::unordered_set<std::string> seen;
	const auto add_state = [this, &normalized_source, &links, &seen](const State& state) {
		for (auto& link : document_links_from_state(state, normalized_source)) {
			const auto key = link.ref_file + "@" + link.ref_range.to_string()
				+ "=>" + link.def_file;
			if (seen.insert(key).second) links.push_back(std::move(link));
		}
	};

	std::lock_guard lock(m_mutex);
	for (const auto& entry : preferred_entries) {
		const auto state = m_states.find(FileSystemSourceProvider::normalize(entry.value).value);
		if (state != m_states.end()) add_state(state->second);
	}
	for (const auto& [_, state] : m_states) add_state(state);

	std::ranges::sort(links, [](const DefinitionLink& a, const DefinitionLink& b) {
		if (a.ref_range.start.line != b.ref_range.start.line) {
			return a.ref_range.start.line < b.ref_range.start.line;
		}
		return a.ref_range.start.column < b.ref_range.start.column;
	});
	return links;
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
	const auto add = [&locations, &seen](
		const std::string& file, const SourceRange& range, const bool spelled_as_declared = true) {
		auto key = file + "@" + range.to_string();
		if (seen.insert(std::move(key)).second) {
			locations.push_back({file, range, spelled_as_declared});
		}
	};

	std::lock_guard lock(m_mutex);
	for (const auto& [_, state] : m_states) {
		for (const auto& reference : references_from_state(state, target)) {
			// Qualifier blocks and function headers carry a self-link so their declaration
			// remains addressable even without usages. Do not expose that implementation
			// detail when the client explicitly excludes declarations.
			const bool is_declaration = reference.ref_file == reference.def_file
				&& reference.ref_range.start.line == reference.def_name_range.start.line
				&& reference.ref_range.start.column == reference.def_name_range.start.column
				&& reference.ref_range.end.line == reference.def_name_range.end.line
				&& reference.ref_range.end.column == reference.def_name_range.end.column;
			if (is_declaration && !include_declaration) continue;
			add(reference.ref_file, reference.ref_range, reference.spelled_as_declared);
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
