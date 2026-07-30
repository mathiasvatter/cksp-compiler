#include "DeprecatedReturnSyntaxAnalyzer.h"

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
	return {
		.kind = Diagnostic::DiagnosticFix::EditKind::Replace,
		.file = m_result_variable->tok.file,
		.range = SourceRange(
			m_function->header->range.end,
			m_result_variable->range.end
		),
		.new_text = ""
	};
}

std::optional<Diagnostic::DiagnosticFix>
DeprecatedReturnSyntaxAnalyzer::make_simple_fix() const {
	if (!m_simple_fix_possible
		|| !m_function
		|| !m_function->header
		|| !m_result_variable
		|| m_terminal_assignments.empty()
		|| m_function->header->tok.file != m_result_variable->tok.file
		|| m_function->header->range.end.line != m_result_variable->range.end.line) {
		return std::nullopt;
	}

	std::vector<Diagnostic::DiagnosticFix::Edit> edits;
	edits.reserve(m_terminal_assignments.size() + 1);
	edits.push_back(remove_header_result_edit());
	for (const auto* assignment : m_terminal_assignments) {
		edits.push_back({
			.kind = Diagnostic::DiagnosticFix::EditKind::Replace,
			.file = assignment->tok.file,
			.range = SourceRange(
				assignment->l_value->range.start,
				assignment->r_value->range.start
			),
			.new_text = "return "
		});
	}

	return Diagnostic::DiagnosticFix{
		.kind = Diagnostic::DiagnosticFix::FixKind::ConvertDeprecatedFunctionReturn,
		.title = "Convert '" + m_function->header->name + "' to return statements",
		.edits = std::move(edits),
		.is_preferred = true
	};
}

std::optional<Diagnostic::DiagnosticFix>
DeprecatedReturnSyntaxAnalyzer::make_fallback_fix() const {
	if (!m_function
		|| !m_function->header
		|| !m_function->body
		|| !m_result_variable
		|| m_function->header->tok.file != m_result_variable->tok.file
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
	const size_t indent_width = body_indent > function_indent
		? body_indent - function_indent
		: 4;
	const char indent_character = indent_width == 1 ? '\t' : ' ';
	edits.push_back({
		.kind = Diagnostic::DiagnosticFix::EditKind::InsertBefore,
		.file = m_result_variable->tok.file,
		.range = first_statement.range,
		.new_text = "declare " + result_name + "\n"
			+ std::string(body_indent, indent_character)
	});
	edits.push_back({
		.kind = Diagnostic::DiagnosticFix::EditKind::InsertBefore,
		.file = m_result_variable->tok.file,
		.range = SourceRange(end_function_start, end_function_start),
		.new_text =
			std::string(
				body_indent > function_indent
					? body_indent - function_indent
					: 0,
				indent_character
			)
			+ "return " + result_name + "\n"
			+ std::string(function_indent, indent_character)
	});


	return Diagnostic::DiagnosticFix{
		.kind = Diagnostic::DiagnosticFix::FixKind::ConvertDeprecatedFunctionReturn,
		.title = "Convert '" + m_function->header->name + "' using a local result",
		.edits = std::move(edits),
		.is_preferred = true
	};
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
		&& node.l_value->tok.file == node.r_value->tok.file;
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
