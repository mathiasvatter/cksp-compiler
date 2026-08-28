//
// Created by Mathias Vatter on 16.08.26.
//

#pragma once

#include <optional>
#include <string>

#include "../ASTNodes/ASTInstructions.h"
#include "../../misc/DiagnosticFixBuilder.h"

/**
 * Diagnostics for calls into SublimeKSP's Task Control Module.
 *
 * TCM is the runtime half of <taskfunc>: <tcm.init> reserves the per-callback stacks and
 * <tcm.wait> waits without losing the frame. CKSP does both without being asked, so the
 * calls have no counterpart to resolve to and would otherwise surface as an ordinary
 * "has not been declared" - the least useful thing to tell someone porting a script.
 *
 * See TaskfuncMigration for the block-level half; a <tcm.wait> written inside a taskfunc
 * is already covered by that fix and never reaches here.
 */
namespace tcm_migration {

	/// The literal a <#pragma> argument can be written from, if the call passes one.
	inline std::optional<int32_t> literal_argument(const NodeFunctionCall& node) {
		if (!node.function or node.function->get_num_args() != 1) return std::nullopt;
		const auto& argument = node.function->get_arg(0);
		if (!argument) return std::nullopt;
		if (const auto* literal = argument->cast<NodeInt>()) return literal->value;
		return std::nullopt;
	}

	/// The diagnostic for a <tcm.*> call, or nothing when the name is not one of TCM's.
	inline std::optional<Diagnostic> make_diagnostic(
		const NodeFunctionCall& node, const std::string& function_name) {
		if (!function_name.starts_with("tcm.")) return std::nullopt;

		auto diagnostic = Diagnostic(
			ErrorType::SyntaxError, "", "",
			node.function ? node.function->tok : node.tok);
		diagnostic.migration_kind = Diagnostic::MigrationKind::TCM;

		if (function_name == "tcm.init") {
			diagnostic.message =
				"Found a call to SublimeKSP's <tcm.init>. CKSP sizes the callback stacks"
				" itself, so there is nothing to initialise; the depth is a compiler option"
				" rather than a runtime call.";
			// The pragma is accepted anywhere, including inside the <on init> the call
			// already sits in, so the whole rewrite is one in-place replacement.
			if (const auto depth = literal_argument(node)) {
				const auto title = "Replace with '#pragma max_callback_depth(" + std::to_string(*depth) + ")'";
				const auto replacement = "#pragma max_callback_depth(" + std::to_string(*depth) + ")";
				diagnostic.fix = DiagnosticFixBuilder(Diagnostic::DiagnosticFix::FixKind::ConvertTCMCall, title)
					.replace(node.tok.file(), node.range, replacement)
					.build();
			} else {
				// A computed depth cannot become a pragma argument: pragmas are read before
				// anything is folded. Naming the option beats offering a fix that would not
				// compile.
				diagnostic.add_message(
					"Set it with <#pragma max_callback_depth(...)> or <--max-callback-depth>"
					" instead. Only a literal can be written there, so this call needs to be"
					" resolved by hand.");
			}
			return diagnostic;
		}

		if (function_name == "tcm.wait") {
			diagnostic.message =
				"Found a call to SublimeKSP's <tcm.wait>. Keeping a function's storage alive"
				" across a wait is what CKSP does anyway, so the plain <wait> builtin is the"
				" whole translation.";
			const auto& function_token = node.function ? node.function->tok : node.tok;
			diagnostic.fix = DiagnosticFixBuilder(Diagnostic::DiagnosticFix::FixKind::ConvertTCMCall, "Replace 'tcm.wait' with 'wait'")
				.replace(function_token, "wait")
				.build();
			return diagnostic;
		}

		diagnostic.message =
			"Found a call into SublimeKSP's Task Control Module (<" + function_name + ">)."
			" CKSP has no TCM: every function already gets its own per-callback storage, so"
			" ordinary functions and the plain <wait> builtin cover what TCM was for.";
		return diagnostic;
	}

} // namespace tcm_migration
