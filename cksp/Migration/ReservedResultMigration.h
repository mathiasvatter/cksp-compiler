//
// Created by Mathias Vatter on 18.08.26.
//

#pragma once

#include <span>
#include <string>
#include <vector>

#include "../Tokenizer/Token.h"
#include "../../misc/DiagnosticFixBuilder.h"

/**
 * The fix for a function whose result is named after the <return> keyword.
 *
 * SublimeKSP has no return statement, so <return> is a free word there and its deprecated
 * result syntax uses it: <function neg(x) -> return> declares a variable, and the body writes
 * to it. CKSP reserves the word, so the declaration and every line touching it are rejected -
 * and the message that the word is reserved says nothing about the construct it came from.
 *
 * The definition is parsed in full anyway, the way a <taskfunc> is, so the rename can cover
 * every place the name stands rather than the first. It stays fatal: the word is a keyword,
 * and no later pass could tell a variable spelled that way apart from a statement.
 *
 * The renamed function still uses the deprecated arrow return, which
 * DeprecatedReturnSyntaxAnalyzer picks up afterwards with a fix of its own - the same
 * layering TaskfuncMigration relies on.
 */
namespace reserved_result_migration {

/// The word CKSP reserves and SublimeKSP does not.
inline const std::string& reserved_name() {
	static const std::string name = get_token_string(token::RETURN);
	return name;
}

/// Whether `token` is the result variable rather than a word that merely reads like it.
///
/// Every <return> the parser met inside such a definition is that variable: the dialect the
/// definition is written in has no return statement to spell. The type is checked all the
/// same, so a string or a comment carrying the word is left alone.
inline bool is_result_token(const Token& token) {
	return (token.type == token::RETURN || token.type == token::KEYWORD)
		&& token.val == reserved_name();
}

/// <return1>, or the next number the definition does not already spell.
inline std::string free_result_name(const std::span<const Token> definition_tokens) {
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
	const auto renamed = free_result_name(definition_tokens);

	DiagnosticFixBuilder fix(Diagnostic::DiagnosticFix::FixKind::RenameReservedResult, "Rename result '" + reserved_name() + "' to '" + renamed + "'");
	bool has_edits = false;
	const Token* result_token = nullptr;
	for (const auto& token : definition_tokens) {
		if (!is_result_token(token) || token.file().empty()) continue;
		if (!result_token) result_token = &token;
		fix.replace(token, renamed);
		has_edits = true;
	}

	auto error = Diagnostic(
		ErrorType::SyntaxError, "", "<return variable>",
		result_token ? *result_token : Token());
	error.migration_kind = Diagnostic::MigrationKind::ReservedResultName;
	error.message =
		"The result of <" + function_name + "> is named <" + reserved_name() + ">, which CKSP"
		" reserves for the <Return> Statement. SublimeKSP has no such statement, so the word is"
		" a name there. Renaming it leaves an ordinary deprecated result, which CKSP can then"
		" convert into a <Return> Statement of its own.";
	if (has_edits) {
		error.fix = fix.build();
	}
	return error;
}

} // namespace reserved_result_migration
