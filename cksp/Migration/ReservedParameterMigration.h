//
// Created by Mathias Vatter on 17.09.26.
//

#pragma once

#include <span>
#include <string>

#include "../Tokenizer/Token.h"
#include "../../misc/DiagnosticFixBuilder.h"

/**
 * The fix for a function parameter named after the <ref> keyword.
 *
 * CKSP spells a pass-by-reference parameter <ref value>, so in a parameter list the word is a
 * qualifier and the name is expected to follow it. SublimeKSP has no such qualifier, which
 * leaves <ref> a free word there, and a ported script uses it for what it reads like - a
 * reference pitch, a reference value. The parser then meets the <)> with no name in hand and
 * says so, which describes where it ended up rather than what is wrong.
 *
 * The definition is parsed in full anyway, the way a <taskfunc> is, so the rename can cover
 * every place the name stands rather than the first. The parser marks those places while it
 * walks the definition: a <ref> that a name does not follow is not a qualifier, and inside
 * the body the word can only be this parameter.
 */
namespace reserved_parameter_migration {

/// The word CKSP reserves and SublimeKSP does not.
inline const std::string& reserved_name() {
	static const std::string name = get_token_string(token::REF);
	return name;
}

/// Whether `token` is the parameter rather than the qualifier of another one.
///
/// The parser rewrites exactly the occurrences that are names into ordinary keyword tokens
/// before it walks the definition, so a <ref> that stayed a <ref> qualifies a parameter and
/// has to keep its spelling.
inline bool is_parameter_token(const Token& token) {
	return token.type == token::KEYWORD && token.val == reserved_name();
}

/// <ref1>, or the next number the definition does not already spell.
inline std::string free_parameter_name(const std::span<const Token> definition_tokens) {
	std::string renamed;
	for (int suffix = 1;; ++suffix) {
		renamed = reserved_name() + std::to_string(suffix);
		const bool taken = std::ranges::any_of(definition_tokens, [&](const Token& token) {
			return token.val == renamed;
		});
		if (!taken) return renamed;
	}
}

/// `definition_tokens` is the range the parser walked, from <function> to <end function>.
inline Diagnostic make_diagnostic(
	const std::string& function_name, const std::span<const Token> definition_tokens) {
	const auto renamed = free_parameter_name(definition_tokens);

	DiagnosticFixBuilder fix(
		Diagnostic::DiagnosticFix::FixKind::RenameReservedParameter,
		"Rename parameter '" + reserved_name() + "' to '" + renamed + "'");
	bool has_edits = false;
	const Token* parameter_token = nullptr;
	for (const auto& token : definition_tokens) {
		if (!is_parameter_token(token) || token.file().empty()) continue;
		if (!parameter_token) parameter_token = &token;
		fix.replace(token, renamed);
		has_edits = true;
	}

	auto error = Diagnostic(
		ErrorType::SyntaxError, "", "<parameter name>",
		parameter_token ? *parameter_token : Token());
	error.migration_kind = Diagnostic::MigrationKind::ReservedParameterName;
	error.message =
		"A parameter of <" + function_name + "> is named <" + reserved_name() + ">, which CKSP"
		" reserves for the pass-by-reference qualifier a parameter list writes before the name."
		" SublimeKSP has no such qualifier, so the word is a name there. Renaming it leaves an"
		" ordinary parameter.";
	if (has_edits) {
		error.fix = fix.build();
	}
	return error;
}

} // namespace reserved_parameter_migration
