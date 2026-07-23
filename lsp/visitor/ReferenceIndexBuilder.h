//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
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
	SourceProvider* m_sources = nullptr;
	mutable std::unordered_map<std::string, std::shared_ptr<const std::string>> m_source_cache;
	std::vector<std::string> m_prefix_stack;

	[[nodiscard]] std::string qualified_name(const std::string& name) const {
		return m_prefix_stack.empty() ? name : m_prefix_stack.back() + "." + name;
	}

	void add_qualifier_links(const Token& reference, const NodeAST& target) const {
		std::vector<std::string> segments;
		std::istringstream names(reference.val);
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
				const auto token = segment_token(reference, offset, segments[i]);
				m_index.add(
					token.file,
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

	std::optional<Token> source_verified_token(const Token& token) const {
		if (!m_sources) return token;
		const auto range = source_range_from_token(token);
		if (!range.is_valid() || range.start.line != range.end.line || range.start.column < 1) {
			return std::nullopt;
		}
		auto cache = m_source_cache.find(token.file);
		if (cache == m_source_cache.end()) {
			auto loaded = m_sources->load(SourceId(token.file));
			if (loaded.is_error()) return token;
			cache = m_source_cache.emplace(token.file, loaded.unwrap().text).first;
		}
		const auto& text = *cache->second;

		size_t offset = 0;
		for (size_t line = 1; line < range.start.line; ++line) {
			offset = text.find('\n', offset);
			if (offset == std::string::npos) return std::nullopt;
			++offset;
		}
		const size_t line_end = std::min(text.find('\n', offset), text.size());
		const size_t start = offset + (range.start.column - 1);
		const size_t end = offset + (range.end.column - 1);
		if (start > end || end > line_end) return std::nullopt;
		const auto actual = text.substr(start, end - start);
		if (actual == token.val) return token;
		return std::nullopt;
	}

	void add_link(const Token& reference, NodeAST& target) const {
		// Builtins/engine variables and synthesized nodes have no real source file.
		if (reference.file.empty() || target.tok.file.empty()) return;
		auto verified_reference = source_verified_token(reference);
		if (!verified_reference) return;
		const auto def_range = declaration_range(target);
		if (!def_range.is_valid()) return;
		add_qualifier_links(*verified_reference, target);

		Token direct_reference = *verified_reference;
		if (verified_reference->val != target.tok.val) {
			const auto dot = verified_reference->val.rfind('.');
			if (dot == std::string::npos || verified_reference->val.substr(dot + 1) != target.tok.val) {
				return;
			}
			direct_reference = segment_token(*verified_reference, dot + 1, target.tok.val);
			auto verified_direct_reference = source_verified_token(direct_reference);
			if (!verified_direct_reference) return;
			direct_reference = *verified_direct_reference;
		}

		// The reference side stays on the token: access-chain members carry per-segment
		// tokens (see the to_method_chain overrides), so the token spans exactly the
		// clickable identifier.
		const auto ref_range = source_range_from_token(direct_reference);
		if (!ref_range.is_valid()) return;
		// the name range carries the exact identifier at the declaration, which rename
		// edits replace; def_range may span the whole header for functions
		m_index.add(
			direct_reference.file, ref_range,
			target.tok.file, def_range,
			source_range_from_token(target.tok));
	}

	void add_link(const NodeAST& reference, NodeAST& target) const {
		add_link(reference.tok, target);
	}

	void record_type_references(NodeAST& node) const {
		if (m_pass != Pass::References || !m_program) return;
		for (const auto& reference : node.type_references) {
			if (!reference.type || reference.type->get_type_kind() != TypeKind::Object) continue;
			auto* definition = NodeReference::get_object_ptr(m_program, reference.type->to_string());
			if (!definition) continue;
			add_link(reference.token, *definition);
		}
	}

	void record_variable(const NodeReference& reference) const {
		if (const auto declaration = reference.get_declaration()) {
			if (declaration->kind == NodeDataStructure::Kind::Builtin) return;
			// Struct desugaring injects a synthetic `self` declaration whose token is the
			// struct name token. Indexing self -> that declaration would make the range-based
			// symbol identity conflate every `self` use with the struct itself.
			if (declaration->name == "self" && declaration->tok.val != "self") return;
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
	explicit ReferenceIndexBuilder(ReferenceIndex& index, const Pass pass, SourceProvider* sources = nullptr)
		: m_index(index), m_pass(pass), m_sources(sources) {}

	NodeAST* visit(NodeVariable& node) override {
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodePointer& node) override {
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeArray& node) override {
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeNDArray& node) override {
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeList& node) override {
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeFunctionHeader& node) override {
		if (m_pass == Pass::Definitions) {
			add_link(node.tok, node);
		}
		record_type_references(node);
		return ASTVisitor::visit(node);
	}

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
				if (node.kind == NodeFunctionCall::Kind::Constructor) {
					// Constructor syntax names the struct (`Preset(...)`), not its __init__
					// method. Give calls, annotations and the declaration one symbol identity.
					if (auto* struct_definition = definition->parent
						? definition->parent->cast<NodeStruct>()
						: nullptr) {
						add_link(*node.function, *struct_definition);
					}
				} else {
					// Jump to the function definition's header (the name at the definition site).
					add_link(*node.function, *definition->header);
				}
			}
		}
		return ASTVisitor::visit(node);
	}
};
