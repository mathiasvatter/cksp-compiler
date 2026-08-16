//
// Created by Mathias Vatter on 16.08.26.
//

#pragma once

#include <string>

#include "../Tokenizer/Token.h"
#include "../../misc/Diagnostic.h"

/**
 * The message for SublimeKSP's <iterate_post_macro> and <literate_post_macro>.
 *
 * CKSP adopted <iterate_macro> and <literate_macro>, which are evaluated while macros are
 * expanded. SublimeKSP's post variants run afterwards, which is what lets their bounds come
 * out of the expansion itself - a macro parameter, or a define written by an earlier macro.
 *
 * No fix is offered, because renaming one is only correct when the bounds happen to be known
 * during expansion already. CKSP rejects a macro parameter there outright ("No variables
 * allowed in Preprocessor"), so for the case the post variant exists for, the rename trades
 * this message for a more obscure one. What to do instead depends on where the bounds come
 * from, which is why the message says so rather than guessing.
 */
namespace macro_migration {

	inline Diagnostic make_post_macro_diagnostic(const Token& macro_token) {
		const bool is_literate = macro_token.val == "literate_post_macro";
		const std::string adopted = is_literate ? "literate_macro" : "iterate_macro";
		const std::string source = is_literate ? "list of literals" : "bounds";

		auto error = Diagnostic(ErrorType::PreprocessorError, "", adopted, macro_token);
		error.message =
			"Found SublimeKSP's <" + macro_token.val + ">. CKSP has <" + adopted + "> only,"
			" which is evaluated while macros are expanded; the post variant runs after that,"
			" so its " + source + " may come out of the expansion itself. If that is already"
			" known at expansion time - a literal or a <define> - use <" + adopted + ">"
			" instead. If it comes from a macro parameter, CKSP has no equivalent: compute the"
			" value into a <define> before the macro, or expand the cases explicitly.";
		return error;
	}

} // namespace macro_migration
