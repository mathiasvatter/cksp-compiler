//
// Created by Mathias Vatter on 18.08.26.
//

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../Tokenizer/Token.h"
#include "../../misc/Diagnostic.h"
#include "../../utils/StringUtils.h"

/**
 * Diagnostics for a name that a SublimeKSP script spells in a case of its own.
 *
 * SublimeKSP resolves names case-insensitively, so a ported script declares <myCounter>
 * and writes <MyCounter>, and every one of those lands on an undeclared-name error here.
 * Unlike the other suggestions the compiler makes, this one is not a guess about what was
 * meant - it is the same identifier - which is what makes it worth offering as an edit.
 *
 * Three cases are told apart, and only the first produces a fix:
 *
 *  - one declaration matches case-insensitively: the written word is corrected to it,
 *  - several do: nothing is corrected, because CKSP would have to pick one of two
 *    declarations that SublimeKSP never had to tell apart. The message names both.
 *  - the word was assembled by a macro: it stands in no file, so the edit has to reach
 *    the argument at the call site instead. See <make_macro_argument_edit>.
 *
 * Anything further apart than case stays an ordinary "did you mean" in the message.
 */
namespace identifier_case_migration {

/// One editable macro argument together with the spelling that corrects it.
struct MacroArgumentEdit {
	Diagnostic::DiagnosticFix::Edit edit;
	std::string written;
	std::string replacement;
};

/// The one substitution span covering `index`, or nothing when none or several do.
inline const TokenSubstitutionSource* source_at(
	const std::vector<TokenSubstitutionSource>& sources, const size_t index) {
	const TokenSubstitutionSource* found = nullptr;
	for (const auto& candidate : sources) {
		if (index < candidate.generated_start
			|| index >= candidate.generated_start + candidate.generated_length) continue;
		if (found) return nullptr;
		found = &candidate;
	}
	return found;
}

/// Two spans stand for the same written word when they point at the same place in the same
/// file. One argument pasted twice produces two spans that have to be corrected together.
inline bool is_same_argument(
	const TokenSubstitutionSource& left, const TokenSubstitutionSource& right) {
	return left.file() == right.file()
		&& left.line == right.line
		&& left.pos == right.pos
		&& left.spelling == right.spelling;
}

/// Whether an edit may touch `source`: it names a place in a file, the user spelled it
/// rather than another expansion, and it still describes the text standing at its span.
inline bool is_editable_span(
	const TokenSubstitutionSource& source, const std::string& generated) {
	return !source.file().empty()
		&& source.editable
		&& source.generated_length == source.spelling.size()
		&& source.generated_start + source.generated_length <= generated.size()
		&& generated.compare(source.generated_start, source.generated_length, source.spelling) == 0;
}

/// Corrects the case of the macro argument that a word was assembled from. The word itself
/// stands in no file - <UIpage_#family#> spells half of it in the macro body and half at the
/// call site - so the edit rewrites the argument, which is what <UIpage_ARP1> needs to
/// resolve after <show_page(arp1)>.
///
/// The candidate spelling is read straight out of `replacement` at the argument's span and
/// then verified by substituting it back into every span the same argument contributed. That
/// one comparison decides the whole rewrite: it fails when a differing character comes from
/// the macro body instead of an argument, when two arguments would have to change at once,
/// and when one argument is pasted twice and the two halves disagree about its case.
inline std::optional<MacroArgumentEdit> make_macro_argument_edit(
	const Token& token,
	const std::string& written,
	const std::string& replacement) {
	if (!token.origin || token.origin->substitution_sources.empty()
		|| written.size() != replacement.size()
		|| token.val != written) return std::nullopt;

	const auto& sources = token.origin->substitution_sources;
	const auto first_difference = std::ranges::mismatch(written, replacement).in1 - written.begin();
	if (static_cast<size_t>(first_difference) >= written.size()) return std::nullopt;

	const auto* argument = source_at(sources, static_cast<size_t>(first_difference));
	if (!argument || !is_editable_span(*argument, written)) return std::nullopt;

	auto corrected = replacement.substr(argument->generated_start, argument->generated_length);
	if (corrected == argument->spelling) return std::nullopt;

	std::string rebuilt = written;
	for (const auto& candidate : sources) {
		if (!is_same_argument(candidate, *argument)) continue;
		if (!is_editable_span(candidate, written)) return std::nullopt;
		rebuilt.replace(candidate.generated_start, candidate.generated_length, corrected);
	}
	if (rebuilt != replacement) return std::nullopt;

	return MacroArgumentEdit{
		.edit = {
			.kind = Diagnostic::DiagnosticFix::EditKind::Replace,
			.file = argument->file(),
			.range = SourceRange(
				{argument->line, argument->pos},
				{argument->line, argument->pos + argument->spelling.length()}),
			.new_text = corrected
		},
		.written = argument->spelling,
		.replacement = std::move(corrected)
	};
}

/// Adds the case correction for `written` to `diagnostic`, if one of `suggestions` is the
/// same identifier in a different case. `suggestions` is expected to name every declaration
/// only once; the guard against a repeated name is what keeps a duplicate from reading as
/// two declarations to tell apart.
inline void apply(
	Diagnostic& diagnostic,
	const std::string& written,
	const std::vector<std::string>& suggestions,
	const Token& token) {
	const auto lower_written = StringUtils::to_lower(written);
	std::vector<std::string> matches;
	for (const auto& suggestion : suggestions) {
		if (suggestion == written) continue;
		if (StringUtils::to_lower(suggestion) != lower_written) continue;
		if (std::ranges::find(matches, suggestion) == matches.end()) {
			matches.push_back(suggestion);
		}
	}

	if (matches.empty()) return;
	diagnostic.migration_kind = Diagnostic::MigrationKind::IdentifierCase;

	if (matches.size() > 1) {
		diagnostic.message += " Case-insensitive lookup is ambiguous between: "
			+ StringUtils::join(matches, ", ") + ".";
		return;
	}

	const auto& replacement = matches.front();
	if (token.origin) {
		if (auto argument_edit = make_macro_argument_edit(token, written, replacement)) {
			diagnostic.fix = Diagnostic::DiagnosticFix{
				.kind = Diagnostic::DiagnosticFix::FixKind::CorrectNameCase,
				.title = "Change macro argument '" + argument_edit->written
					+ "' to '" + argument_edit->replacement + "'",
				.edits = {std::move(argument_edit->edit)},
				.is_preferred = true
			};
			return;
		}
		diagnostic.message += " Its spelling comes from macro expansion and cannot be rewritten safely.";
		return;
	}
	if (token.val != written) return;

	diagnostic.fix = Diagnostic::DiagnosticFix{
		.kind = Diagnostic::DiagnosticFix::FixKind::CorrectNameCase,
		.title = "Change '" + written + "' to '" + replacement + "'",
		.edits = {{
			.kind = Diagnostic::DiagnosticFix::EditKind::Replace,
			.file = token.file(),
			.range = source_range_from_token(token),
			.new_text = replacement
		}},
		.is_preferred = true
	};
}

} // namespace identifier_case_migration
