#include "CodeActionProvider.h"

#include "RequestParams.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace {
	bool requests_quick_fixes(const JSONObject* context) {
		const auto* only_value = context ? context->get("only") : nullptr;
		if (!only_value) {
			return true;
		}

		const auto* only = only_value->as<JSONArray>();
		if (!only) {
			return false;
		}
		for (size_t index = 0; index < only->size(); ++index) {
			const auto* value = only->at(index);
			const auto* kind = value ? value->as<JSONString>() : nullptr;
			if (kind && kind->value == "quickfix") {
				return true;
			}
		}
		return false;
	}

	/**
	 * Whether the request asks for the fix-all source action.
	 *
	 * Code action kinds are hierarchical, so a request for `source` covers `source.fixAll` too.
	 * An absent `only` is the lightbulb asking what it can offer here; a source action rewrites
	 * the whole document and does not belong in that list, so it is only ever offered on request.
	 */
	bool requests_fix_all(const JSONObject* context) {
		const auto* only_value = context ? context->get("only") : nullptr;
		const auto* only = only_value ? only_value->as<JSONArray>() : nullptr;
		if (!only) {
			return false;
		}
		for (size_t index = 0; index < only->size(); ++index) {
			const auto* value = only->at(index);
			const auto* kind = value ? value->as<JSONString>() : nullptr;
			if (kind && (kind->value == "source" || kind->value == "source.fixAll")) {
				return true;
			}
		}
		return false;
	}

	/// One edit of a fix, in the coordinates it will be sent in.
	struct PlannedEdit {
		std::string uri;
		int64_t start_line = 0;
		int64_t start_character = 0;
		int64_t end_line = 0;
		int64_t end_character = 0;
		std::string new_text;

		friend bool operator==(const PlannedEdit&, const PlannedEdit&) = default;
	};

	std::optional<PlannedEdit> read_planned_edit(const JSONObject* edit_data) {
		const auto* uri = lsp::string_at(edit_data, "targetUri");
		const auto* range = lsp::object_at(edit_data, "range");
		const auto* new_text = lsp::string_at(edit_data, "newText");
		const auto* start = lsp::object_at(range, "start");
		const auto* end = lsp::object_at(range, "end");
		if (!uri || !new_text || !start || !end) {
			return std::nullopt;
		}

		const auto start_line = start->get_int("line");
		const auto start_character = start->get_int("character");
		const auto end_line = end->get_int("line");
		const auto end_character = end->get_int("character");
		if (!start_line || !start_character || !end_line || !end_character) {
			return std::nullopt;
		}

		return PlannedEdit{
			uri->value,
			*start_line,
			*start_character,
			*end_line,
			*end_character,
			new_text->value
		};
	}

	/// -1, 0 or 1 for a position before, at, or after another.
	int compare_positions(
		const int64_t left_line,
		const int64_t left_character,
		const int64_t right_line,
		const int64_t right_character) {
		if (left_line != right_line) {
			return left_line < right_line ? -1 : 1;
		}
		if (left_character != right_character) {
			return left_character < right_character ? -1 : 1;
		}
		return 0;
	}

	/**
	 * Whether two edits rewrite a common piece of text.
	 *
	 * A workspace edit is applied against the unchanged document, so two edits that overlap
	 * would corrupt each other. Ranges that merely touch do not overlap; an insertion - an
	 * empty range - overlaps a range that contains its position, and another insertion at the
	 * same position.
	 */
	bool edits_overlap(const PlannedEdit& left, const PlannedEdit& right) {
		if (left.uri != right.uri) {
			return false;
		}

		const bool left_is_empty =
			compare_positions(left.start_line, left.start_character, left.end_line, left.end_character) == 0;
		const bool right_is_empty =
			compare_positions(right.start_line, right.start_character, right.end_line, right.end_character) == 0;

		if (left_is_empty && right_is_empty) {
			return compare_positions(
				left.start_line, left.start_character, right.start_line, right.start_character) == 0;
		}
		if (left_is_empty) {
			return compare_positions(
					left.start_line, left.start_character, right.start_line, right.start_character) >= 0
				&& compare_positions(
					left.start_line, left.start_character, right.end_line, right.end_character) <= 0;
		}
		if (right_is_empty) {
			return edits_overlap(right, left);
		}
		return compare_positions(
				left.start_line, left.start_character, right.end_line, right.end_character) < 0
			&& compare_positions(
				right.start_line, right.start_character, left.end_line, left.end_character) < 0;
	}

	std::unique_ptr<JSONObject> make_text_edit(const PlannedEdit& edit) {
		auto start = std::make_unique<JSONObject>();
		start->add("line", std::make_unique<JSONInt>(edit.start_line));
		start->add("character", std::make_unique<JSONInt>(edit.start_character));

		auto end = std::make_unique<JSONObject>();
		end->add("line", std::make_unique<JSONInt>(edit.end_line));
		end->add("character", std::make_unique<JSONInt>(edit.end_character));

		auto range = std::make_unique<JSONObject>();
		range->add("start", std::move(start));
		range->add("end", std::move(end));

		auto text_edit = std::make_unique<JSONObject>();
		text_edit->add("range", std::move(range));
		text_edit->add("newText", std::make_unique<JSONString>(edit.new_text));
		return text_edit;
	}

	/**
	 * Every fix of the document merged into one source action, or nothing when none applies.
	 *
	 * A fix is taken whole or not at all: applying half of one leaves the code in a state
	 * neither the compiler nor the user asked for. A fix whose edit overlaps one already taken
	 * is therefore left out entirely - it stays available as its own quick fix, and a second
	 * fix-all run picks it up once the first round has been applied and analyzed. The same edit
	 * arriving twice, which happens when one mistake is reported at several call sites, is not
	 * a conflict: it is already covered by the edit that was taken.
	 */
	std::unique_ptr<JSONObject> make_fix_all_action(const std::vector<Diagnostic>& diagnostics) {
		std::vector<PlannedEdit> taken;

		for (const auto& diagnostic : diagnostics) {
			if (!diagnostic.fix || diagnostic.fix->edits.empty()) {
				continue;
			}

			std::vector<PlannedEdit> candidates;
			bool fix_is_applicable = true;
			for (const auto& edit : diagnostic.fix->edits) {
				const auto planned = read_planned_edit(
					DiagnosticPublisher::make_lsp_edit_data(edit).get());
				if (!planned) {
					fix_is_applicable = false;
					break;
				}

				const auto is_duplicate = [&](const std::vector<PlannedEdit>& edits) {
					for (const auto& existing : edits) {
						if (existing == *planned) {
							return true;
						}
					}
					return false;
				};
				if (is_duplicate(taken) || is_duplicate(candidates)) {
					continue;
				}

				for (const auto& existing : taken) {
					if (edits_overlap(existing, *planned)) {
						fix_is_applicable = false;
						break;
					}
				}
				if (!fix_is_applicable) {
					break;
				}
				candidates.push_back(*planned);
			}

			if (!fix_is_applicable) {
				continue;
			}
			for (auto& candidate : candidates) {
				taken.push_back(std::move(candidate));
			}
		}

		if (taken.empty()) {
			return nullptr;
		}

		std::map<std::string, std::unique_ptr<JSONArray>> edits_by_uri;
		for (const auto& edit : taken) {
			auto& uri_edits = edits_by_uri[edit.uri];
			if (!uri_edits) {
				uri_edits = std::make_unique<JSONArray>();
			}
			uri_edits->add(make_text_edit(edit));
		}

		auto changes = std::make_unique<JSONObject>();
		for (auto& [uri, edits] : edits_by_uri) {
			changes->add(uri, std::move(edits));
		}
		auto workspace_edit = std::make_unique<JSONObject>();
		workspace_edit->add("changes", std::move(changes));

		auto action = std::make_unique<JSONObject>();
		action->add("title", std::make_unique<JSONString>("Apply all CKSP fixes"));
		action->add("kind", std::make_unique<JSONString>("source.fixAll"));
		action->add("edit", std::move(workspace_edit));
		return action;
	}

	std::unique_ptr<JSONObject> make_code_action(const JSONValue& diagnostic_value) {
		const auto* diagnostic = diagnostic_value.as<JSONObject>();
		const auto* data = lsp::object_at(diagnostic, "data");
		const auto* title = lsp::string_at(data, "title");
		const auto* edits_value = data ? data->get("edits") : nullptr;
		const auto* fix_edits = edits_value ? edits_value->as<JSONArray>() : nullptr;
		if (!title || !fix_edits || fix_edits->size() == 0) {
			return nullptr;
		}

		std::map<std::string, std::unique_ptr<JSONArray>> edits_by_uri;
		for (size_t index = 0; index < fix_edits->size(); ++index) {
			const auto* edit_data_value = fix_edits->at(index);
			const auto* edit_data = edit_data_value
				? edit_data_value->as<JSONObject>()
				: nullptr;
			const auto* target_uri = lsp::string_at(edit_data, "targetUri");
			const auto* edit_range = lsp::object_at(edit_data, "range");
			const auto* new_text = lsp::string_at(edit_data, "newText");
			if (!target_uri || !edit_range || !new_text) {
				return nullptr;
			}

			auto edit = std::make_unique<JSONObject>();
			edit->add("range", edit_range->clone());
			edit->add("newText", std::make_unique<JSONString>(new_text->value));

			auto& uri_edits = edits_by_uri[target_uri->value];
			if (!uri_edits) {
				uri_edits = std::make_unique<JSONArray>();
			}
			uri_edits->add(std::move(edit));
		}

		auto changes = std::make_unique<JSONObject>();
		for (auto& [uri, edits] : edits_by_uri) {
			changes->add(uri, std::move(edits));
		}
		auto workspace_edit = std::make_unique<JSONObject>();
		workspace_edit->add("changes", std::move(changes));

		auto action_diagnostics = std::make_unique<JSONArray>();
		action_diagnostics->add(diagnostic_value.clone());

		auto action = std::make_unique<JSONObject>();
		action->add("title", std::make_unique<JSONString>(title->value));
		action->add("kind", std::make_unique<JSONString>("quickfix"));
		if (const auto* preferred = data->get<JSONBool>("isPreferred")) {
			action->add("isPreferred", std::make_unique<JSONBool>(preferred->value));
		}
		action->add("diagnostics", std::move(action_diagnostics));
		action->add("edit", std::move(workspace_edit));
		return action;
	}
} // namespace

JSONArray CodeActionProvider::provide(
	const JSONObject* params,
	const DiagnosticPublisher& publisher) {
	JSONArray actions;
	const auto* context = lsp::object_at(params, "context");

	if (requests_quick_fixes(context)) {
		const auto* diagnostics_value = context ? context->get("diagnostics") : nullptr;
		if (const auto* diagnostics =
			diagnostics_value ? diagnostics_value->as<JSONArray>() : nullptr) {
			for (size_t index = 0; index < diagnostics->size(); ++index) {
				const auto* diagnostic = diagnostics->at(index);
				if (!diagnostic) {
					continue;
				}
				if (auto action = make_code_action(*diagnostic)) {
					actions.add(std::move(action));
				}
			}
		}
	}

	if (requests_fix_all(context)) {
		const auto* text_document = lsp::object_at(params, "textDocument");
		if (const auto* uri = lsp::string_at(text_document, "uri")) {
			if (auto action = make_fix_all_action(
				publisher.diagnostics_for(source_from_uri(uri->value)))) {
				actions.add(std::move(action));
			}
		}
	}

	return actions;
}
