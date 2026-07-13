//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <string>

#include "../../cksp/ASTVisitor/ASTVisitor.h"
#include "../../cksp/ASTNodes/ASTReferences.h"
#include "../../cksp/Source/ReferenceIndex.h"
#include "../../cksp/Source/SourceProvider.h"

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

	/// The token spans exactly the identifier. Access-chain members carry per-segment tokens
	/// (see the to_method_chain overrides), and struct method/constructor headers have an
	/// unreliable node.range, so both the reference and the declaration use the token.
	static SourceRange node_range(const NodeAST& node) {
		return source_range_from_token(node.tok);
	}

	void add_link(const NodeAST& reference, const NodeAST& target) const {
		// Builtins/engine variables and synthesized nodes have no real source file.
		if (reference.tok.file.empty() || target.tok.file.empty()) return;
		const auto ref_range = node_range(reference);
		const auto def_range = node_range(target);
		if (!ref_range.is_valid() || !def_range.is_valid()) return;
		m_index.add(
			FileSystemSourceProvider::normalize(reference.tok.file).value, ref_range,
			FileSystemSourceProvider::normalize(target.tok.file).value, def_range);
	}

	void record_variable(const NodeReference& reference) const {
		if (const auto declaration = reference.get_declaration()) {
			if(declaration->kind == NodeDataStructure::Kind::Builtin) return;
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
		if (node.function and !node.is_builtin_kind()) {
			if (const auto definition = node.get_definition()) {
				// Jump to the function definition's header (the name at the definition site).
				add_link(*node.function, *definition->header);
			}
		}
		return ASTVisitor::visit(node);
	}
};
