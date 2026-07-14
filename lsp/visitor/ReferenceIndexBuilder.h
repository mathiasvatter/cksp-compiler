//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "../../cksp/ASTVisitor/ASTVisitor.h"
#include "../../cksp/ASTNodes/ASTReferences.h"
#include "../../cksp/Source/ReferenceIndex.h"
#include "../../cksp/Source/SourceProvider.h"

/**
 * Builds the ReferenceIndex in two AST passes.
 *
 * The Definitions pass runs before desugaring and preserves namespace/family/const block
 * locations. The References pass runs after semantic resolution and connects references,
 * including individual qualifier segments, to their declarations.
 */
class ReferenceIndexBuilder final : public ASTVisitor {
public:
	enum class Pass {
		Definitions,
		References
	};

private:
	ReferenceIndex& m_index;
	Pass m_pass;
	std::vector<std::string> m_prefix_stack;

	[[nodiscard]] std::string qualified_name(const std::string& name) const {
		return m_prefix_stack.empty() ? name : m_prefix_stack.back() + "." + name;
	}

	void add_qualifier_links(const NodeAST& reference, const NodeAST& target) const {
		std::vector<std::string> segments;
		std::istringstream names(reference.tok.val);
		for (std::string segment; std::getline(names, segment, '.');) {
			segments.push_back(std::move(segment));
		}
		if (segments.size() < 2) return;

		std::string prefix;
		size_t offset = 0;
		for (size_t i = 0; i + 1 < segments.size(); ++i) {
			if (!prefix.empty()) prefix += '.';
			prefix += segments[i];
			if (const auto definition = m_index.qualifier_definition(prefix, target.tok.file)) {
				const auto token = segment_token(reference.tok, offset, segments[i]);
				m_index.add(
					FileSystemSourceProvider::normalize(token.file).value,
					source_range_from_token(token),
					definition->file,
					definition->range);
			}
			offset += segments[i].size() + 1;
		}
	}

	/// Function definitions present their whole header (name, parameters, parenthesis) as
	/// the definition range, like other lsps do; the parser maintains that range on the
	/// header node. Every other declaration jumps to its name token.
	static SourceRange declaration_range(NodeAST& node) {
		if (node.cast<NodeFunctionHeader>() && node.range.is_valid()) {
			return node.range;
		}
		return source_range_from_token(node.tok);
	}

	void add_link(const NodeAST& reference, NodeAST& target) const {
		// Builtins/engine variables and synthesized nodes have no real source file.
		if (reference.tok.file.empty() || target.tok.file.empty()) return;
		// The reference side stays on the token: access-chain members carry per-segment
		// tokens (see the to_method_chain overrides), so the token spans exactly the
		// clickable identifier.
		const auto ref_range = source_range_from_token(reference.tok);
		const auto def_range = declaration_range(target);
		if (!ref_range.is_valid() || !def_range.is_valid()) return;
		add_qualifier_links(reference, target);
		m_index.add(
			FileSystemSourceProvider::normalize(reference.tok.file).value, ref_range,
			FileSystemSourceProvider::normalize(target.tok.file).value, def_range);
	}

	void record_variable(const NodeReference& reference) const {
		if (const auto declaration = reference.get_declaration()) {
			if (declaration->kind == NodeDataStructure::Kind::Builtin) return;
			add_link(reference, *declaration);
		}
	}

	template<typename Node>
	NodeAST* record_definition(Node& node, const std::string& local_name) {
		if (m_pass != Pass::Definitions) return ASTVisitor::visit(node);
		const auto name = qualified_name(local_name);
		m_index.add_qualifier_definition(name, node.tok);
		m_prefix_stack.push_back(name);
		ASTVisitor::visit(node);
		m_prefix_stack.pop_back();
		return &node;
	}

public:
	explicit ReferenceIndexBuilder(ReferenceIndex& index, const Pass pass)
		: m_index(index), m_pass(pass) {}

	NodeAST* visit(NodeVariableRef& node) override {
		if (m_pass == Pass::References) record_variable(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeArrayRef& node) override {
		if (m_pass == Pass::References) record_variable(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeNDArrayRef& node) override {
		if (m_pass == Pass::References) record_variable(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodePointerRef& node) override {
		if (m_pass == Pass::References) record_variable(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeListRef& node) override {
		if (m_pass == Pass::References) record_variable(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeNamespace& node) override {
		return record_definition(node, node.prefix);
	}

	NodeAST* visit(NodeFamily& node) override {
		return record_definition(node, node.prefix);
	}

	NodeAST* visit(NodeConst& node) override {
		return record_definition(node, node.name);
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		if (m_pass == Pass::References && node.function and !node.is_builtin_kind()) {
			if (const auto definition = node.get_definition()) {
				// Jump to the function definition's header (the name at the definition site).
				add_link(*node.function, *definition->header);
			}
		}
		return ASTVisitor::visit(node);
	}
};
