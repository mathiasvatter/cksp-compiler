#include "DeprecatedReturnSyntaxAnalyzer.h"
#include "../../../misc/DiagnosticFixBuilder.h"

#include <string>
#include <utility>

static size_t END_FUNCTION_LENGTH = get_token_string(token::END_FUNCTION).size();

Diagnostic DeprecatedReturnSyntaxAnalyzer::make_warning(const Token& result_token) {
	auto warning = Diagnostic(
		ErrorType::SyntaxError,
		"",
		"<Return> Statement",
		result_token
	);
	warning.message = "Deprecated return syntax used in function definition. For type safety, using"
					  " multiple return values or returning arrays, use <return> statement syntax instead.";
	return warning;
}

bool DeprecatedReturnSyntaxAnalyzer::references_result(NodeAST& node) const {
	return m_result_variable
		&& node.collect_free_vars().contains(m_result_variable->name);
}

Diagnostic::DiagnosticFix::Edit
DeprecatedReturnSyntaxAnalyzer::remove_header_result_edit() const {
	const SourceRange result_range(m_function->header->range.end, m_result_variable->range.end);
	return DiagnosticFixBuilder::replace_edit(m_result_variable->tok.file(), result_range, "");
}

std::optional<Diagnostic::DiagnosticFix>
DeprecatedReturnSyntaxAnalyzer::make_simple_fix() const {
	if (!m_simple_fix_possible
		|| !m_function
		|| !m_function->header
		|| !m_result_variable
		|| m_terminal_assignments.empty()
		|| m_function->header->tok.file() != m_result_variable->tok.file()
		|| m_function->header->range.end.line != m_result_variable->range.end.line) {
		return std::nullopt;
	}

	std::vector<Diagnostic::DiagnosticFix::Edit> edits;
	edits.reserve(m_terminal_assignments.size() + 1);
	edits.push_back(remove_header_result_edit());
	for (const auto* assignment : m_terminal_assignments) {
		const SourceRange assignment_range(assignment->l_value->range.start, assignment->r_value->range.start);
		edits.push_back(DiagnosticFixBuilder::replace_edit(assignment->tok.file(), assignment_range, "return "));
	}

	return DiagnosticFixBuilder(Diagnostic::DiagnosticFix::FixKind::ConvertDeprecatedFunctionReturn, "Convert '" + m_function->header->name + "' to return statements")
		.add_edits(std::move(edits))
		.build();
}

std::optional<Diagnostic::DiagnosticFix>
DeprecatedReturnSyntaxAnalyzer::make_fallback_fix() const {
	if (!m_function
		|| !m_function->header
		|| !m_function->body
		|| !m_result_variable
		|| m_function->header->tok.file() != m_result_variable->tok.file()
		|| m_function->header->range.end.line != m_result_variable->range.end.line
		|| m_function->range.end.column <= END_FUNCTION_LENGTH) {
		return std::nullopt;
	}

	const SourcePosition end_function_start{
		.line = m_function->range.end.line,
		.column = m_function->range.end.column - END_FUNCTION_LENGTH
	};
	const size_t function_indent = end_function_start.column > 0
		? end_function_start.column - 1
		: 0;
	const std::string result_name = m_result_variable->tok.val;
	std::vector<Diagnostic::DiagnosticFix::Edit> edits;
	edits.push_back(remove_header_result_edit());

	const auto& first_statement = *m_function->body->get_first_statement();
	const size_t body_indent = first_statement.range.start.column > 0
		? first_statement.range.start.column - 1
		: function_indent + 4;
	const size_t indent_step = body_indent > function_indent
		? body_indent - function_indent
		: 4;
	const auto body_indentation = indentation(body_indent, indent_step);
	const auto function_indentation = indentation(function_indent, indent_step);
	const auto step_indentation = indentation(
		body_indent > function_indent ? body_indent - function_indent : 0, indent_step);
	const auto declaration_text = "declare " + result_name + "\n" + body_indentation;
	const auto return_text = step_indentation + "return " + result_name + "\n" + function_indentation;
	edits.push_back(DiagnosticFixBuilder::insert_before_edit(m_result_variable->tok.file(), first_statement.range, declaration_text));
	edits.push_back(DiagnosticFixBuilder::insert_before_edit(m_result_variable->tok.file(), SourceRange(end_function_start, end_function_start), return_text));


	return DiagnosticFixBuilder(Diagnostic::DiagnosticFix::FixKind::ConvertDeprecatedFunctionReturn, "Convert '" + m_function->header->name + "' using a local result")
		.add_edits(std::move(edits))
		.build();
}

void DeprecatedReturnSyntaxAnalyzer::analyze(NodeFunctionDefinition& function) {
	auto warning = make_warning(function.return_variable.value()->tok);
	m_function = &function;
	m_result_variable = function.return_variable.value()->cast<NodeVariable>();
	m_simple_fix_possible = m_result_variable && function.body;
	m_terminal_assignments.clear();

	if (m_result_variable && function.body && !function.body->empty()) {
		function.body->accept(*this);
		warning.fix = make_simple_fix();
		if (!warning.fix) {
			warning.fix = make_fallback_fix();
		}
	}
	warning.report(diagnostics());

	m_function = nullptr;
	m_result_variable = nullptr;
	m_simple_fix_possible = false;
	m_terminal_assignments.clear();
}

NodeAST* DeprecatedReturnSyntaxAnalyzer::visit(NodeBlock& node) {
	if (node.empty()) {
		m_simple_fix_possible = false;
		return &node;
	}

	for (size_t index = 0; index + 1 < node.statements.size(); ++index) {
		if (references_result(*node.statements[index])) {
			m_simple_fix_possible = false;
			return &node;
		}
	}

	auto& terminal = node.get_last_statement();
	if (auto* assignment = terminal->cast<NodeSingleAssignment>()) {
		assignment->accept(*this);
	} else if (auto* conditional = terminal->cast<NodeIf>()) {
		conditional->accept(*this);
	} else {
		m_simple_fix_possible = false;
	}
	return &node;
}

NodeAST* DeprecatedReturnSyntaxAnalyzer::visit(NodeSingleAssignment& node) {
	const bool assigns_result = node.l_value
		&& node.r_value
		&& m_result_variable
		&& node.l_value->name == m_result_variable->name
		&& !references_result(*node.r_value)
		&& node.l_value->range.start.line == node.r_value->range.start.line
		&& node.l_value->tok.file() == node.r_value->tok.file();
	if (assigns_result) {
		m_terminal_assignments.push_back(&node);
	} else {
		m_simple_fix_possible = false;
	}
	return &node;
}

NodeAST* DeprecatedReturnSyntaxAnalyzer::visit(NodeIf& node) {
	if (!node.condition
		|| references_result(*node.condition)
		|| !node.if_body
		|| !node.else_body
		|| node.else_body->empty()) {
		m_simple_fix_possible = false;
		return &node;
	}
	node.if_body->accept(*this);
	node.else_body->accept(*this);
	return &node;
}
