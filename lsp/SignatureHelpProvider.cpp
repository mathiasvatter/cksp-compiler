#include "SignatureHelpProvider.h"

#include <string>

std::unique_ptr<JSONObject> SignatureHelpProvider::help(
	const std::vector<SourceId>& preferred_entries,
	const lsp::CallSiteQuery& query,
	const SourceId& source,
	const size_t line,
	const size_t character) const {
	auto callables = m_completion.callables(
		preferred_entries,
		query.qualifier,
		query.callable,
		source,
		line,
		character
	);
	if (callables.empty()) return nullptr;

	auto signatures = std::make_unique<JSONArray>();
	for (const auto& callable : callables) {
		auto signature = std::make_unique<JSONObject>();
		const auto label = callable.detail.empty()
			? callable.label + callable.parameters
			: callable.detail;
		signature->add("label", std::make_unique<JSONString>(label));

		auto parameters = std::make_unique<JSONArray>();
		for (const auto& parameter_label : callable.parameter_labels) {
			auto parameter = std::make_unique<JSONObject>();
			parameter->add("label", std::make_unique<JSONString>(parameter_label));
			parameters->add(std::move(parameter));
		}
		signature->add("parameters", std::move(parameters));
		signatures->add(std::move(signature));
	}

	auto result = std::make_unique<JSONObject>();
	result->add("signatures", std::move(signatures));
	result->add("activeSignature", std::make_unique<JSONInt>(0));
	// With no parameters, every index is out of range and the protocol tells clients to
	// ignore it. Omitting the optional field expresses that state without a sentinel.
	if (!callables.front().parameter_labels.empty()) {
		result->add(
			"activeParameter",
			std::make_unique<JSONInt>(static_cast<long long>(query.active_parameter))
		);
	}
	return result;
}
