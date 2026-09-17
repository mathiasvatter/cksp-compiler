//
// Created by Mathias Vatter on 16.08.26.
//

#pragma once

#include <optional>
#include <string>

#include "../Tokenizer/Token.h"
#include "../../misc/DiagnosticFixBuilder.h"

/**
 * Diagnostics for a SublimeKSP compiler pragma, which is written inside a <{...}> block.
 *
 * To CKSP that block is a comment, so the line is swallowed whole and the pragma does
 * nothing - and nothing says so. For <save_compiled_source> that is the worst shape a
 * failure can take: the compile reports success while the output goes to the default path
 * instead of the one the line names.
 *
 * Reported as a warning and left in the source on purpose. The same line is a comment to
 * CKSP and a pragma to SublimeKSP, so a file carrying one still compiles under both. Only
 * the quick fix, which the user has to ask for, commits the file to CKSP.
 */
namespace pragma_migration {

	/**
	 * The CKSP pragma a SublimeKSP <compile_with>/<compile_without> toggle turns into, or
	 * nothing where CKSP has no counterpart - which is the case for most of them. CKSP has no
	 * say over whitespace, the compile date or SublimeKSP's extra syntax checks, so those
	 * lines carry nothing to port and are only worth deleting.
	 */
	inline std::optional<std::string> cksp_pragma_for(const std::string& toggle, const bool enabled) {
		if (toggle == "optimize_code") {
			// CKSP optimizes at <standard> unless told otherwise, so the enabled form only
			// writes down what already happens; the disabled one is the one that changes a compile.
			return "#pragma optimize(\"" + std::string(enabled ? "standard" : "none") + "\")";
		}
		if (toggle == "combine_callbacks") {
			return "#pragma combine_callbacks(" + std::string(enabled ? "true" : "false") + ")";
		}
		if (toggle == "compact_variables") {
			// The nearest CKSP has, not the same thing: SublimeKSP shortens the identifiers to
			// keep the script small, CKSP's obfuscation replaces them with unreadable ones of
			// its own length. What both do is take the source's names out of the output.
			return "#pragma obfuscate(" + std::string(enabled ? "true" : "false") + ")";
		}
		return std::nullopt;
	}

	/// `option` is the pragma's name, `argument` everything between it and the closing brace.
	inline Diagnostic make_diagnostic(
		const Token& pragma_token, const std::string& option, const std::string& argument) {
		auto warning = Diagnostic(ErrorType::CompileWarning, "", "", pragma_token);
		warning.migration_kind = Diagnostic::MigrationKind::SublimePragma;

		if (option == "save_compiled_source" and !argument.empty()) {
			warning.message =
				"Found the SublimeKSP pragma <save_compiled_source>. CKSP reads this line as the"
				" comment it is written inside, so it has no effect and the compiled output goes"
				" to the default path. CKSP spells it <#pragma output_path(\"...\")>.";
			// The whole <{#pragma ...}> span is replaced, including its braces. The path is
			// quoted rather than escaped: CKSP only treats a backslash as an escape before the
			// quote character itself, so a Windows path survives as written.
			warning.fix = DiagnosticFixBuilder(Diagnostic::DiagnosticFix::FixKind::ConvertSublimePragma, "Replace with '#pragma output_path(...)'")
				.replace(pragma_token, "#pragma output_path(\"" + argument + "\")")
				.build();
			return warning;
		}

		if (option == "compile_with" or option == "compile_without") {
			if (const auto replacement = cksp_pragma_for(argument, option == "compile_with")) {
				warning.message =
					"Found the SublimeKSP pragma <" + option + " " + argument + ">. CKSP reads"
					" this line as the comment it is written inside, so it has no effect here."
					" CKSP spells it <" + *replacement + ">.";
				warning.fix = DiagnosticFixBuilder(
						Diagnostic::DiagnosticFix::FixKind::ConvertSublimePragma,
						"Replace with '" + *replacement + "'")
					.replace(pragma_token, *replacement)
					.build();
				return warning;
			}
		}

		// Naming a CKSP pragma after the SublimeKSP option would be an invention: <#pragma
		// compile_with(...)> does not exist, and following that advice trades a warning for an
		// error. The six that do exist are named instead, so the reader can check for himself.
		const auto construct = argument.empty() ? option : option + " " + argument;
		warning.message =
			"Found a SublimeKSP compiler pragma (<" + construct + ">). CKSP reads this line as"
			" the comment it is written inside, so it has no effect here, and CKSP has no pragma"
			" that covers it - the line can be deleted. CKSP's own pragmas are <output_path>,"
			" <optimize>, <pass_by>, <combine_callbacks>, <obfuscate> and <max_callback_depth>,"
			" written without the braces.";
		return warning;
	}

} // namespace pragma_migration
