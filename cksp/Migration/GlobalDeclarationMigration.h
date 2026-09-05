//
// Created by Mathias Vatter on 18.08.26.
//

#pragma once

#include <optional>
#include <string>

#include "../ASTNodes/ASTInstructions.h"
#include "../../misc/DiagnosticFixBuilder.h"

/**
 * The fix for a global declaration that a function initializes from one of its locals.
 *
 * SublimeKSP evaluates <declare global stored := control> where it stands, so the
 * initializer sees the surrounding function's parameters and locals. CKSP hoists the whole
 * declaration into global scope before resolving it, where those names do not exist - the
 * script is rejected with an undeclared-name error naming a variable that is plainly there.
 *
 * The rewrite keeps the declaration global and leaves the assignment behind in the function,
 * which is what restores SublimeKSP's evaluation order in a source CKSP accepts.
 *
 * Only a plain <NodeVariable> is offered this. An array or a list carries its size and its
 * initializer list in the same statement, so splitting one is not the single-line edit this
 * is; those keep the message and no fix.
 */
namespace global_declaration_migration {

/// The width of one indentation step in the function the declaration was written in.
///
/// Hoisting moved the declaration out of that function, so its own nesting is gone from the
/// parent chain and only its column survives - and a column cannot tell two tabs from two
/// spaces. The step has to be measured between two lines that are one level apart, which is
/// a function header and the first statement of its body: the same pair
/// <DeprecatedReturnSyntaxAnalyzer> measures, found here by source position because the
/// declaration no longer sits inside it. Zero when no function on that line can be found,
/// which leaves the caller with the column and its one usable reading.
inline size_t indent_step_of(const NodeSingleDeclaration& declaration) {
	const auto* program = get_parent_of_type<NodeProgram>(declaration);
	if (!program) return 0;

	const NodeFunctionDefinition* innermost = nullptr;
	for (const auto& definition : program->function_definitions) {
		if (!definition || !definition->range.is_valid() || !definition->body) continue;
		if (definition->tok.file() != declaration.tok.file()) continue;
		if (declaration.tok.line < definition->range.start.line
			|| declaration.tok.line > definition->range.end.line) continue;
		// A nested definition starts later than the one containing it, so the last one that
		// still contains the line is the innermost - the level the step is measured against.
		if (!innermost || definition->range.start.line > innermost->range.start.line) {
			innermost = definition.get();
		}
	}
	if (!innermost || innermost->body->empty()) return 0;

	const auto& first_statement = *innermost->body->get_first_statement();
	if (!first_statement.range.is_valid()) return 0;
	const auto function_column = innermost->range.start.column;
	const auto body_column = first_statement.range.start.column;
	return body_column > function_column ? body_column - function_column : 0;
}

inline std::optional<Diagnostic::DiagnosticFix> make_fix(const NodeReference& reference) {
	auto* declaration = get_parent_of_type<NodeSingleDeclaration>(reference);
	if (!declaration
		|| declaration->kind != NodeInstruction::Kind::HoistedGlobal
		|| !declaration->value
		|| !declaration->variable
		|| !declaration->variable->cast<NodeVariable>()
		|| declaration->variable->data_type == DataType::Const) {
		return std::nullopt;
	}

	// Only a reference in the initializer justifies moving the assignment. A reference in
	// a type annotation or another part of the declaration must not trigger this rewrite.
	if (!is_in_subtree(reference, declaration->value.get())) return std::nullopt;

	const auto& variable = *declaration->variable;
	const auto& value = *declaration->value;
	if (!variable.range.is_valid()
		|| !value.range.is_valid()
		|| declaration->tok.file().empty()
		|| variable.tok.file() != declaration->tok.file()
		|| value.tok.file() != declaration->tok.file()
		|| declaration->tok.origin
		|| variable.tok.origin
		|| value.tok.origin) {
		return std::nullopt;
	}

	const auto& separator_start = variable.range.end;
	const auto& separator_end = value.range.start;
	if (separator_end.line < separator_start.line
		|| (separator_end.line == separator_start.line
			&& separator_end.column < separator_start.column)) {
		return std::nullopt;
	}

	// The assignment takes the declaration's place on its own line and has to line up with
	// it, at whatever depth that line was written.
	const auto indentation = indentation_at(declaration->tok.pos, indent_step_of(*declaration));
	const std::string variable_name = variable.tok.val;

	return DiagnosticFixBuilder(Diagnostic::DiagnosticFix::FixKind::SplitGlobalDeclarationAssignment, "Split global declaration of '" + variable_name + "' from its assignment")
		.replace(declaration->tok.file(), SourceRange(separator_start, separator_end), "\n" + indentation + variable_name + " := ")
		.build();
}

} // namespace global_declaration_migration
