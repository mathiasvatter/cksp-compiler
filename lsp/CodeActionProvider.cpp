#include "CodeActionProvider.h"

#include "RequestParams.h"

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

	std::unique_ptr<JSONObject> make_code_action(const JSONValue& diagnostic_value) {
		const auto* diagnostic = diagnostic_value.as<JSONObject>();
		const auto* data = lsp::object_at(diagnostic, "data");
		const auto* title = lsp::string_at(data, "title");
		const auto* target_uri = lsp::string_at(data, "targetUri");
		const auto* edit_range = lsp::object_at(data, "editRange");
		const auto* new_text = lsp::string_at(data, "newText");
		if (!title || !target_uri || !edit_range || !new_text) {
			return nullptr;
		}

		auto edit = std::make_unique<JSONObject>();
		edit->add("range", edit_range->clone());
		edit->add("newText", std::make_unique<JSONString>(new_text->value));

		auto edits = std::make_unique<JSONArray>();
		edits->add(std::move(edit));
		auto changes = std::make_unique<JSONObject>();
		changes->add(target_uri->value, std::move(edits));
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

JSONArray CodeActionProvider::provide(const JSONObject* params) {
	JSONArray actions;
	const auto* context = lsp::object_at(params, "context");
	if (!requests_quick_fixes(context)) {
		return actions;
	}

	const auto* diagnostics_value = context ? context->get("diagnostics") : nullptr;
	const auto* diagnostics = diagnostics_value ? diagnostics_value->as<JSONArray>() : nullptr;
	if (!diagnostics) {
		return actions;
	}

	for (size_t index = 0; index < diagnostics->size(); ++index) {
		const auto* diagnostic = diagnostics->at(index);
		if (!diagnostic) {
			continue;
		}
		if (auto action = make_code_action(*diagnostic)) {
			actions.add(std::move(action));
		}
	}
	return actions;
}
