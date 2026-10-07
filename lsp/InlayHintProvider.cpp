#include "InlayHintProvider.h"

#include <algorithm>
#include <cctype>

#include "CallSiteScanner.h"

namespace {

/// LSP InlayHintKind.
constexpr int TYPE_HINT = 1;
constexpr int PARAMETER_HINT = 2;
constexpr size_t MAX_LABEL_LENGTH = 10;

class LineIndex {
	std::vector<size_t> m_starts{0};

public:
	explicit LineIndex(const std::string_view text) {
		for (size_t i = 0; i < text.size(); ++i) {
			if (text[i] == '\n') m_starts.push_back(i + 1);
		}
	}

	[[nodiscard]] std::pair<size_t, size_t> line_and_character(const size_t offset) const {
		const auto next = std::ranges::upper_bound(m_starts, offset);
		const auto line = static_cast<size_t>(next - m_starts.begin()) - 1;
		return {line, offset - m_starts[line]};
	}

	[[nodiscard]] std::unique_ptr<JSONObject> position_of(const size_t offset) const {
		const auto [line, character] = line_and_character(offset);
		auto position = std::make_unique<JSONObject>();
		position->add("line", std::make_unique<JSONInt>(static_cast<long long>(line)));
		position->add("character", std::make_unique<JSONInt>(static_cast<long long>(character)));
		return position;
	}
};

std::string lowercase(const std::string_view text) {
	std::string lowered(text);
	std::ranges::transform(lowered, lowered.begin(), [](const unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return lowered;
}

}

std::string InlayHintProvider::short_name_of(const std::string& parameter_label) {
	const auto cut = parameter_label.find_first_of(":-?");
	if (cut != std::string::npos && cut <= MAX_LABEL_LENGTH) {
		return parameter_label.substr(0, cut);
	}
	if (parameter_label.size() > MAX_LABEL_LENGTH) {
		return parameter_label.substr(0, MAX_LABEL_LENGTH) + "…";
	}
	return parameter_label;
}

bool InlayHintProvider::argument_names_parameter(
	const std::string_view argument, const std::string& short_name) {
	const auto lowered_argument = lowercase(argument);
	const auto lowered_name = lowercase(short_name);
	std::string token;
	const auto token_in_argument = [&] {
		const bool found = token.size() >= 2 && lowered_argument.find(token) != std::string::npos;
		token.clear();
		return found;
	};
	for (const char c : lowered_name) {
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
			token += c;
		} else if (token_in_argument()) {
			return true;
		}
	}
	return token_in_argument();
}

JSONArray InlayHintProvider::hints(
	const std::vector<SourceId>& preferred_entries,
	const std::string_view text,
	const size_t begin,
	const size_t end,
	const SourceId& source,
	const std::vector<TypeHint>& type_hints) const {
	JSONArray hints;
	const LineIndex lines(text);

	for (const auto& type_hint : type_hints) {
		const auto offset = lsp::source_text::offset_at(
			text, type_hint.position.get_lsp_line(), type_hint.position.get_lsp_char());
		if (!offset || *offset < begin || *offset > end) continue;
		auto hint = std::make_unique<JSONObject>();
		hint->add("position", lines.position_of(*offset));
		hint->add("label", std::make_unique<JSONString>(": " + type_hint.type));
		hint->add("kind", std::make_unique<JSONInt>(TYPE_HINT));
		hints.add(std::move(hint));
	}

	for (const auto& call : lsp::call_sites_in(text, begin, end)) {
		if (call.arguments.empty()) continue;
		const auto [line, character] = lines.line_and_character(call.name_offset);
		const auto callables = m_completion.callables(
			preferred_entries, call.qualifier, call.callable, source, line, character);
		if (callables.empty()) continue;
		const auto& parameters = callables.front().parameter_labels;

		const auto count = std::min(call.arguments.size(), parameters.size());
		for (size_t i = 0; i < count; ++i) {
			const auto& argument = call.arguments[i];
			if (argument.text.empty()) continue;
			const auto short_name = short_name_of(parameters[i]);
			if (short_name.empty() || argument_names_parameter(argument.text, short_name)) continue;

			auto data = std::make_unique<JSONObject>();
			data->add("parameterCount", std::make_unique<JSONInt>(
				static_cast<long long>(parameters.size())));

			auto hint = std::make_unique<JSONObject>();
			hint->add("position", lines.position_of(argument.offset));
			hint->add("label", std::make_unique<JSONString>(short_name + ":"));
			hint->add("kind", std::make_unique<JSONInt>(PARAMETER_HINT));
			hint->add("tooltip", std::make_unique<JSONString>(parameters[i]));
			hint->add("paddingRight", std::make_unique<JSONBool>(true));
			hint->add("data", std::move(data));
			hints.add(std::move(hint));
		}
	}
	return hints;
}
