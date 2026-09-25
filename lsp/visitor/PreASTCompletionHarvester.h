//
// Created for LSP completion support.
//

#pragma once

#include <string>
#include <vector>

#include "../../cksp/Preprocessor/PreAST/PreASTNodes/PreAST.h"
#include "../../cksp/Source/CompletionIndex.h"

namespace lsp {

/**
 * Harvests <define> and <macro> definitions into a CompletionIndex.
 *
 * These never reach the AST: the preprocessor substitutes them away before parsing, so
 * the normal harvest cannot see them. PreNodeProgram keeps both in flat vectors, which
 * makes this a plain walk rather than another visitor.
 *
 * Preprocessor definitions are global by construction, so none of them is scoped.
 */
namespace detail {

/// "(a, b)" for a parameterised definition, empty when it has no parameter list.
[[nodiscard]] inline std::vector<std::string> parameter_labels_of(PreNodeList* args) {
	std::vector<std::string> labels;
	if (!args) return labels;
	labels.reserve(args->params.size());
	for (const auto& parameter : args->params) {
		if (parameter) labels.push_back(parameter->get_string());
	}
	return labels;
}

[[nodiscard]] inline std::string parameters_of(
	const std::vector<std::string>& labels, const bool has_parenth) {
	if (!has_parenth && labels.empty()) return {};
	std::string parameters = "(";
	for (size_t i = 0; i < labels.size(); ++i) {
		if (i) parameters += ", ";
		parameters += labels[i];
	}
	return parameters + ")";
}

/// Collapses a definition body to one line for the detail slot, since a define is most
/// useful when you can see what it stands for.
[[nodiscard]] inline std::string one_line(const std::string& text, const size_t limit = 60) {
	std::string collapsed;
	bool pending_space = false;
	for (const char c : text) {
		if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
			pending_space = !collapsed.empty();
			continue;
		}
		if (pending_space) {
			collapsed += ' ';
			pending_space = false;
		}
		collapsed += c;
	}
	if (collapsed.size() > limit) collapsed = collapsed.substr(0, limit) + "...";
	return collapsed;
}

}

inline void harvest_preprocessor_definitions(PreNodeProgram& program, CompletionIndex& index) {
	for (const auto& define : program.define_statements) {
		if (!define || !define->header) continue;
		const auto name = define->header->get_name();
		if (name.empty()) continue;
		const auto parameter_labels = detail::parameter_labels_of(define->header->args.get());
		const auto parameters = detail::parameters_of(
			parameter_labels, define->header->has_parenth);

		std::string signature = "define " + name + parameters;
		// A parameterless define stands for a value; showing it is the point.
		if (parameters.empty() && define->body) {
			if (const auto body = detail::one_line(define->body->get_string());
				!body.empty()) {
				signature += " := " + body;
			}
		}
		index.add_declaration({
			.name = name,
			.parameters = parameters,
			.detail = signature,
			.kind = parameters.empty() ? CompletionKind::Constant : CompletionKind::Function,
			.category = "define",
			.parameter_labels = parameter_labels,
		});
	}

	for (const auto& macro : program.macro_definitions) {
		if (!macro || !macro->header) continue;
		const auto name = macro->header->get_name();
		if (name.empty()) continue;
		const auto parameter_labels = detail::parameter_labels_of(macro->header->args.get());
		const auto parameters = detail::parameters_of(
			parameter_labels, macro->header->has_parenth);
		index.add_declaration({
			.name = name,
			.parameters = parameters,
			.detail = "macro " + name + parameters,
			.kind = CompletionKind::Function,
			.category = "macro",
			.parameter_labels = parameter_labels,
		});
	}
}

}
