//
// Created by Claude for go-to-definition support.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/ASTReferences.h"
#include "../Source/ReferenceIndex.h"
#include "../Source/SourceProvider.h"

/**
 * Walks a resolved AST and records reference -> declaration links into a ReferenceIndex.
 *
 * Variable-like references carry their declaration weak_ptr (populated by ASTVariableChecking).
 * Function calls carry the resolved definition on the NodeFunctionCall itself, so they are
 * handled there rather than through the header reference. Run this after the final variable
 * checking pass, once every declaration is resolved. Only meaningful in LSP mode.
 */
class ReferenceIndexBuilder final : public ASTVisitor {
	ReferenceIndex& m_index;

	/// The reference token spans exactly the clicked identifier; a node's `range` is not
	/// reliable for every reference kind (function header refs get a one-column range).
	static SourceRange reference_range(const NodeAST& node) {
		return source_range_from_token(node.tok);
	}

	static SourceRange declaration_range(const NodeAST& node) {
		if (node.range.is_valid()) return node.range;
		return source_range_from_token(node.tok);
	}

	void add_link(const NodeAST& reference, const NodeAST& target) {
		// Builtins/engine variables and synthesized nodes have no real source file.
		if (reference.tok.file.empty() || target.tok.file.empty()) return;
		const auto ref_range = reference_range(reference);
		const auto def_range = declaration_range(target);
		if (!ref_range.is_valid() || !def_range.is_valid()) return;
		m_index.add(
			FileSystemSourceProvider::normalize(reference.tok.file).value, ref_range,
			FileSystemSourceProvider::normalize(target.tok.file).value, def_range);
	}

	void record_variable(const NodeReference& reference) {
		if (const auto declaration = reference.get_declaration()) {
			add_link(reference, *declaration);
		}
	}

public:
	explicit ReferenceIndexBuilder(ReferenceIndex& index) : m_index(index) {}

	NodeAST* visit(NodeVariableRef& node) override { record_variable(node); return ASTVisitor::visit(node); }
	NodeAST* visit(NodeArrayRef& node) override { record_variable(node); return ASTVisitor::visit(node); }
	NodeAST* visit(NodeNDArrayRef& node) override { record_variable(node); return ASTVisitor::visit(node); }
	NodeAST* visit(NodePointerRef& node) override { record_variable(node); return ASTVisitor::visit(node); }
	NodeAST* visit(NodeListRef& node) override { record_variable(node); return ASTVisitor::visit(node); }

	NodeAST* visit(NodeFunctionCall& node) override {
		if (node.function) {
			if (const auto definition = node.get_definition()) {
				// Jump to the function definition's header (the name at the definition site).
				add_link(*node.function, *definition->header);
			} else if (const auto declaration = node.function->get_declaration()) {
				add_link(*node.function, *declaration);
			}
		}
		return ASTVisitor::visit(node);
	}
};
