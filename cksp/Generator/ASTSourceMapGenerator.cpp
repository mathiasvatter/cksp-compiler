#include "ASTSourceMapGenerator.h"

#include <fstream>
#include <memory>
#include <utility>

#include "../../JSON/ast/JSONValue.h"
#include "version.h"

ASTSourceMapGenerator::ASTSourceMapGenerator(std::string generated_header, std::string entry_file, std::vector<std::string> output_files)
	: m_generated_header(std::move(generated_header)),
	  m_entry_file(std::move(entry_file)),
	  m_output_files(std::move(output_files)) {}

bool ASTSourceMapGenerator::has_source(const NodeAST& node) {
	return !node.tok.file.empty() && node.range.is_valid();
}

SourceRange ASTSourceMapGenerator::statement_range(const NodeAST& statement, const NodeAST& end) {
	const auto start = source_range_from_token(statement.tok);
	if (statement.tok.file.empty()
		|| statement.tok.file != end.tok.file
		|| !start.is_valid()
		|| !end.range.is_valid()
		|| start.start.line != end.range.start.line
		|| end.range.start.line != end.range.end.line) {
		return end.range;
	}
	return SourceRange{start, end.range};
}

void ASTSourceMapGenerator::record(const NodeAST& node) {
	record(node, node.range);
}

void ASTSourceMapGenerator::record(const NodeAST& node, const SourceRange& range) {
	if (node.tok.file.empty() || !range.is_valid() || m_mappings.contains(m_generated_line)) {
		return;
	}
	m_mappings.emplace(m_generated_line, Mapping{
		.generated_line = m_generated_line,
		.source = node.tok.file,
		.source_range = range
	});
}

void ASTSourceMapGenerator::record_with_fallback(const NodeAST& preferred, const NodeAST& fallback) {
	if (has_source(preferred)) {
		record(preferred);
	} else {
		record(fallback);
	}
}

NodeAST* ASTSourceMapGenerator::visit(NodeProgram& node) {
	m_program = &node;
	m_mappings.clear();
	m_generated_line = 1;

	// ASTGenerator always emits its compiled timestamp/version header first.
	advance_line();

	node.callbacks[0]->accept(*this);
	for (const auto& function : node.function_definitions) {
		function->accept(*this);
	}
	for (size_t i = 1; i < node.callbacks.size(); ++i) {
		node.callbacks[i]->accept(*this);
	}

	// ASTGenerator emits one final blank line.
	advance_line();
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeStatement& node) {
	if (node.statement->cast<NodeDeadCode>()) {
		return &node;
	}
	node.statement->accept(*this);
	// ASTGenerator terminates every non-dead statement after its child visitor.
	advance_line();
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeSingleDeclaration& node) {
	NodeAST* source = node.value
		? node.value.get()
		: node.variable.get();
	const auto range = statement_range(node, *source);
	if (has_source(*source) && range.is_valid()) {
		record(*source, range);
	} else {
		record(node);
	}
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeSingleAssignment& node) {
	const auto range = statement_range(node, *node.r_value);
	if (has_source(*node.r_value) && range.is_valid()) {
		record(*node.r_value, range);
	} else {
		record(node);
	}
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeFunctionCall& node) {
	record(node);
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeBlock& node) {
	for (const auto& statement : node.statements) {
		statement->accept(*this);
	}
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeIf& node) {
	record_with_fallback(*node.condition, node);
	advance_line();
	node.if_body->accept(*this);
	if (!node.else_body->statements.empty()) {
		record_with_fallback(*node.else_body, node);
		advance_line();
		node.else_body->accept(*this);
	}
	// record(node);
	// The wrapping NodeStatement accounts for the newline after "end if".
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeWhile& node) {
	record_with_fallback(*node.condition, node);
	advance_line();
	node.body->accept(*this);
	// record(node);
	// The wrapping NodeStatement accounts for the newline after "end while".
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeSelect& node) {
	record_with_fallback(*node.expression, node);
	advance_line();
	for (const auto& [case_values, body] : node.cases) {
		if (!case_values.empty()) {
			record_with_fallback(*case_values.front(), node);
		} else {
			record(node);
		}
		advance_line();
		body->accept(*this);
	}
	// record(node);
	// The wrapping NodeStatement accounts for the newline after "end select".
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeCallback& node) {
	record(node);
	advance_line();
	node.statements->accept(*this);
	record(node);
	advance_line();
	return &node;
}

NodeAST* ASTSourceMapGenerator::visit(NodeFunctionDefinition& node) {
	record_with_fallback(*node.header, node);
	advance_line();
	node.body->accept(*this);
	record(node);
	advance_line();
	return &node;
}

void ASTSourceMapGenerator::generate(const std::string& path) const {
	JSONObject root;
	root.add("version", std::make_unique<JSONInt>(1));
	root.add("compilerVersion", std::make_unique<JSONString>(COMPILER_VERSION));
	root.add("generatedHeader", std::make_unique<JSONString>(m_generated_header));
	root.add("entry", std::make_unique<JSONString>(m_entry_file));
	root.add("generatedLineBase", std::make_unique<JSONInt>(1));

	auto outputs = std::make_unique<JSONArray>();
	for (const auto& output : m_output_files) {
		outputs->add(std::make_unique<JSONString>(output));
	}
	root.add("outputs", std::move(outputs));

	auto mappings = std::make_unique<JSONArray>();
	for (const auto& [_, mapping] : m_mappings) {
		auto mapping_json = std::make_unique<JSONObject>();
		mapping_json->add(
			"generatedLine",
			std::make_unique<JSONInt>(static_cast<long long>(mapping.generated_line)));
		mapping_json->add("source", std::make_unique<JSONString>(mapping.source));
		mapping_json->add("sourceRange", mapping.source_range.get_lsp_range());
		mappings->add(std::move(mapping_json));
	}
	root.add("mappings", std::move(mappings));

	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) {
		std::cerr << "Could not open source map file. " << path << std::endl;
		return;
	}
	output << static_cast<const JSONValue&>(root).get_string() << '\n';
}
