//
// Created by Mathias Vatter on 16.08.26.
//

#pragma once

#include "../Tokenizer/Token.h"
#include "../../misc/Diagnostic.h"

/**
 * The message for a SublimeKSP <property> block.
 *
 * A property there is a global pseudo variable: reading the name calls its <get>, assigning
 * to it calls its <set>. CKSP has no such construct, and unlike <taskfunc> there is no
 * mechanical rewrite to offer - what replaces it depends on what the property was for. A
 * value computed from an object belongs to that object as a pair of methods; a global one
 * with no type to belong to is what a <define> is for. Both are named here rather than
 * guessed at.
 *
 * The token exists only so this message can be given. Nothing in the compiler parses a
 * property block.
 */
namespace property_migration {

	inline Diagnostic make_diagnostic(const Token& property_token) {
		auto error = Diagnostic(ErrorType::SyntaxError, "", "", property_token);
		error.migration_kind = Diagnostic::MigrationKind::Property;
		error.message =
			"Found a SublimeKSP <property>. CKSP has no property construct. A value computed"
			" from an object belongs to that object: give the struct a pair of methods and"
			" call them, <note.freq()> to read and <note.set_freq(x)> to write. A global"
			" property, which belongs to no type, is what a <define> is for.";
		return error;
	}

} // namespace property_migration
