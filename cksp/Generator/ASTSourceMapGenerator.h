#pragma once

#include <map>
#include <string>
#include <vector>

#include "../ASTVisitor/ASTVisitor.h"

/**
 * Builds a line-level mapping from the generated KSP output back to the final
 * AST's original source locations.
 *
 * The visitor mirrors ASTGenerator's line-producing traversal. It deliberately
 * runs after ASTGenerator so the exact generated header can be stored as a
 * build marker without producing a second timestamp.
 */
class ASTSourceMapGenerator final : public ASTVisitor {
	struct Mapping {
		size_t generated_line;
		std::string source;
		SourceRange source_range;
	};

	std::string m_generated_header;
	std::string m_entry_file;
	std::vector<std::string> m_output_files;
	std::map<size_t, Mapping> m_mappings;
	size_t m_generated_line = 1;

	[[nodiscard]] static bool has_source(const NodeAST& node);
	void record(const NodeAST& node);
	void record_with_fallback(const NodeAST& preferred, const NodeAST& fallback);
	void advance_line() { ++m_generated_line; }

public:
	ASTSourceMapGenerator(
		std::string generated_header,
		std::string entry_file,
		std::vector<std::string> output_files);

	NodeAST* visit(NodeProgram& node) override;
	NodeAST* visit(NodeStatement& node) override;
	NodeAST* visit(NodeBlock& node) override;
	NodeAST* visit(NodeIf& node) override;
	NodeAST* visit(NodeWhile& node) override;
	NodeAST* visit(NodeSelect& node) override;
	NodeAST* visit(NodeCallback& node) override;
	NodeAST* visit(NodeFunctionDefinition& node) override;

	void generate(const std::string& path) const;
};
