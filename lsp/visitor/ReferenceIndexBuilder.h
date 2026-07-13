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

	/// Function definitions present their whole header (name, parameters, parenthesis) as
	/// the definition range, like other lsps do; the parser maintains that range on the
	/// header node. Every other declaration jumps to its name token.
	static SourceRange declaration_range(const NodeAST& node) {
		if (node.get_node_type() == NodeType::FunctionHeader && node.range.is_valid()) {
			return node.range;
		}
		return source_range_from_token(node.tok);
	}

	void add_link(const NodeAST& reference, const NodeAST& target) const {
		// Builtins/engine variables and synthesized nodes have no real source file.
		if (reference.tok.file.empty() || target.tok.file.empty()) return;
		// The reference side stays on the token: access-chain members carry per-segment
		// tokens (see the to_method_chain overrides), so the token spans exactly the
		// clickable identifier.
		const auto ref_range = source_range_from_token(reference.tok);
		const auto def_range = declaration_range(target);
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
