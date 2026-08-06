#include "CompletionProvider.h"

#include <unordered_set>

void CompletionProvider::publish(const SourceId& entry, CompletionIndex index, const bool successful) {
	const auto normalized = FileSystemSourceProvider::normalize(entry.value);
	auto shared = std::make_shared<const CompletionIndex>(std::move(index));

	std::lock_guard lock(m_mutex);
	auto& state = m_states[normalized.value];
	state.current = shared;
	// A failed analysis stops before the index is harvested, so it carries no
	// members. Keeping the previous successful snapshot is what makes completion
	// work while the buffer is mid-edit.
	if (successful && !shared->empty()) {
		state.last_successful = std::move(shared);
	}
}

void CompletionProvider::erase(const SourceId& entry) {
	const auto normalized = FileSystemSourceProvider::normalize(entry.value);
	std::lock_guard lock(m_mutex);
	m_states.erase(normalized.value);
}

std::vector<CompletionMember> CompletionProvider::members(
	const std::vector<SourceId>& preferred_entries,
	const std::vector<std::string>& chain,
	const SourceId& source,
	const size_t line,
	const size_t character) const {
	const auto file = FileSystemSourceProvider::normalize(source.value).value;

	std::lock_guard lock(m_mutex);
	std::vector<CompletionMember> found;
	std::unordered_set<std::string> seen;

	for (const auto& entry : preferred_entries) {
		const auto state = m_states.find(FileSystemSourceProvider::normalize(entry.value).value);
		if (state == m_states.end()) continue;
		const auto& index = state->second.last_successful
			? state->second.last_successful
			: state->second.current;
		if (!index) continue;
		// An empty chain means a bare identifier is being typed.
		auto members = chain.empty()
			? index->visible_symbols(file, line, character)
			: index->members_of(chain, file, line, character);
		// A chain that names no qualifier may still name an instance: <inst.>, <self.>.
		if (members.empty() && !chain.empty()) {
			members = index->instance_members_of(chain, file, line, character);
		}
		// Last: a name that merely contains dots, like <macro nks.init()>.
		if (members.empty() && !chain.empty()) {
			members = index->dotted_members_of(chain, file, line, character);
		}
		for (auto& member : members) {
			if (seen.insert(member.label).second) found.push_back(std::move(member));
		}
	}
	return found;
}

std::string CompletionProvider::category_of(const CompletionKind kind) {
	switch (kind) {
		case CompletionKind::Method: return "static function";
		case CompletionKind::Function: return "function";
		case CompletionKind::Field: return "family";
		case CompletionKind::Variable: return "variable";
		case CompletionKind::Module: return "namespace";
		case CompletionKind::Constant: return "const";
		case CompletionKind::Struct: return "struct";
	}
	return {};
}

/// Shaped like the items cksp-tools builds: labelDetails carries the parameter list and
/// the construct word, and the documentation is a cksp code block with the signature.
std::unique_ptr<JSONObject> CompletionProvider::make_item(const CompletionMember& member) {
	auto item = std::make_unique<JSONObject>();
	item->add("label", std::make_unique<JSONString>(member.label));
	item->add("kind", std::make_unique<JSONInt>(static_cast<int>(member.kind)));

	auto label_details = std::make_unique<JSONObject>();
	if (!member.parameters.empty()) {
		label_details->add("detail", std::make_unique<JSONString>(member.parameters));
	}
	if (const auto category = member.category.empty() ? category_of(member.kind) : member.category;
		!category.empty()) {
		label_details->add("description", std::make_unique<JSONString>(category));
	}
	item->add("labelDetails", std::move(label_details));

	if (!member.detail.empty()) {
		item->add("detail", std::make_unique<JSONString>(member.detail));
	}
	// Only a callable's detail is a full signature worth rendering as a block; for a
	// variable it is the bare type, which the detail slot already shows. cksp-tools
	// likewise documents only entries that have a signature.
	if (!member.parameters.empty() && !member.detail.empty()) {
		auto documentation = std::make_unique<JSONObject>();
		documentation->add("kind", std::make_unique<JSONString>("markdown"));
		documentation->add("value", std::make_unique<JSONString>(
			"```cksp\n" + member.detail + "\n```"));
		item->add("documentation", std::move(documentation));
	}
	return item;
}

JSONArray CompletionProvider::items(
	const std::vector<SourceId>& preferred_entries,
	const std::vector<std::string>& chain,
	const SourceId& source,
	const size_t line,
	const size_t character) const {
	JSONArray items;
	for (const auto& member : members(preferred_entries, chain, source, line, character)) {
		items.add(make_item(member));
	}
	return items;
}
