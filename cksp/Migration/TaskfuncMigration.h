//
// Created by Mathias Vatter on 16.08.26.
//

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "../Tokenizer/Token.h"
#include "../../misc/DiagnosticFixBuilder.h"

/**
 * Collects the edits that turn a SublimeKSP <taskfunc> block into a plain CKSP function
 * while the parser walks the block.
 *
 * CKSP has no taskfunc because it needs none: dimension expansion already gives every
 * function its own per-callback storage, so an ordinary function survives being re-entered
 * across a <wait>. The construct is therefore rejected - but rejecting it as an unknown
 * one tells a SublimeKSP user that their working script is a compiler bug. The block is
 * parsed in full instead, and the diagnostic carries the whole rewrite as a quick fix.
 *
 * A converted <taskfunc f(...) -> result> still uses the deprecated arrow return, which
 * DeprecatedReturnSyntaxAnalyzer picks up afterwards with a fix of its own. The two stay
 * layered on purpose: everything collected here is a plain token replacement.
 */
class TaskfuncMigration {
public:
	explicit TaskfuncMigration(const Token& taskfunc_token)
		: m_taskfunc_token(taskfunc_token) {}

	/// <taskfunc> and its <end taskfunc>.
	void add_keyword_edits(const Token& end_token) {
		add_replacement(m_taskfunc_token, "function");
		add_replacement(end_token, "end function");
	}

	/// SublimeKSP spells an in-out parameter <var x> and a write-only one <out x>. Both
	/// become <ref x>: CKSP has no write-only parameter, and passing an <out> by reference
	/// is the reading that is correct either way.
	void add_param_modifier_edit(const Token& modifier) {
		add_replacement(modifier, "ref");
	}

	/// <tcm.wait> is TCM's stack-preserving wait. Preserving the frame is what CKSP does
	/// anyway, so the plain builtin is the whole translation.
	void add_tcm_wait_edit(const Token& call) {
		add_replacement(call, "wait");
	}

	[[nodiscard]] bool empty() const { return m_edits.empty(); }

	[[nodiscard]] Diagnostic make_diagnostic(const std::string& function_name) const {
		auto error = Diagnostic(ErrorType::ParseError, "", "", m_taskfunc_token);
		error.migration_kind = Diagnostic::MigrationKind::Taskfunc;
		error.message =
			"Found a SublimeKSP <taskfunc>. CKSP has no taskfunc because it needs none: every"
			" function already gets its own per-callback storage, so it is safe to re-enter"
			" across a <wait>. Write it as a plain <function ... end function> instead;"
			" <var> and <out> parameters become <ref>, and <tcm.wait> becomes <wait>.";
		DiagnosticFixBuilder fix(Diagnostic::DiagnosticFix::FixKind::ConvertTaskfuncToFunction, "Convert taskfunc '" + function_name + "' to a function");
		fix.add_edits(m_edits);
		error.fix = fix.build();
		return error;
	}

private:
	void add_replacement(const Token& token, std::string new_text) {
		m_edits.push_back(DiagnosticFixBuilder::replace_edit(token, std::move(new_text)));
	}

	Token m_taskfunc_token;
	std::vector<Diagnostic::DiagnosticFix::Edit> m_edits;
};
