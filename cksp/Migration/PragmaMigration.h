//
// Created by Mathias Vatter on 16.08.26.
//

#pragma once

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

		warning.message =
			"Found a SublimeKSP compiler pragma (<" + option + ">). CKSP reads this line as the"
			" comment it is written inside, so it has no effect here. CKSP's own pragmas are"
			" written without the braces, as <#pragma " + option + "(...)>; check whether one"
			" covers this, or delete the line.";
		return warning;
	}

} // namespace pragma_migration
