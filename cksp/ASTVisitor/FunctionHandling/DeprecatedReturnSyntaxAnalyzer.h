#pragma once

#include "../ASTVisitor.h"

#include <optional>
#include <vector>

/**
 * Reports deprecated function result syntax and provides either direct return
 * edits or a semantics-preserving local-result migration.
 */
class DeprecatedReturnSyntaxAnalyzer final : public ASTVisitor {
	NodeFunctionDefinition* m_function = nullptr;
	NodeVariable* m_result_variable = nullptr;
	bool m_simple_fix_possible = false;
	std::vector<NodeSingleAssignment*> m_terminal_assignments;

	[[nodiscard]] Diagnostic::DiagnosticFix::Edit remove_header_result_edit() const;
	[[nodiscard]] std::optional<Diagnostic::DiagnosticFix> make_simple_fix() const;
	[[nodiscard]] std::optional<Diagnostic::DiagnosticFix> make_fallback_fix() const;
	[[nodiscard]] bool references_result(NodeAST& node) const;

	NodeAST* visit(NodeBlock& node) override;
	NodeAST* visit(NodeSingleAssignment& node) override;
	NodeAST* visit(NodeIf& node) override;

public:
	explicit DeprecatedReturnSyntaxAnalyzer(NodeProgram* program) {
		set_program(program);
	}

	void analyze(NodeFunctionDefinition& function);

	[[nodiscard]] static Diagnostic make_warning(const Token& result_token);
};
