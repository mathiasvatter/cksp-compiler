//
// Created by Mathias Vatter on 10.07.25.
//

#pragma once


#include <ranges>

#include "ASTDesugaring.h"
#include "../ASTVisitor/ASTNoVisitor.h"

struct NamespaceData {
	std::unordered_set<std::string> variables; // all variables declared in this namespace
	std::vector<NodePrefix::PrefixSegment> path; // path to the namespace, e.g. "a.b.c" for namespace "c" in "a.b"
};

/**
 * Prepending prefix to variables and references that were declared in namespace
 */
class DesugarNamespace final : public ASTDesugaring {
	std::vector<NodePrefix::PrefixSegment> prefix;
	std::vector<std::unordered_set<std::string>> m_namespace_variables;
	// collects all basenames of variables by the namespace prefix
	std::unordered_map<std::string, std::unique_ptr<NamespaceData>> namespace_data;
	std::unordered_set<std::string> all_prefixed_variables; // all variables that have been prefixed with a namespace

	void add_namespace_prefix(NodeDataStructure& var) {
		if (m_namespace_variables.empty()) return;

		// Register unqualified name in the current (innermost) scope
		const std::string base = basename_of(var.name, prefix);
		m_namespace_variables.back().insert(base);

		// Keyed by the full path, not by the innermost name: two blocks may share a
		// name (<audio.mixer> and <midi.mixer>) and must stay distinct, otherwise the
		// second one's members are attributed to the first one's path.
		const auto path_key = path_string(prefix);
		const auto it = namespace_data.find(path_key);
		if (it == namespace_data.end()) {
			// create new namespace data for this prefix
			auto data = std::make_unique<NamespaceData>();
			data->variables.insert(base);
			data->path = prefix;
			namespace_data[path_key] = std::move(data);
		} else {
			it->second->variables.insert(base);
		}

		for (const auto& namespace_prefix : prefix) {
			var.add_prefix(namespace_prefix);
		}
		var.name = StringUtils::join_apply(
			prefix,
			[](const NodePrefix::PrefixSegment& namespace_prefix) { return namespace_prefix.token.val; },
			"."
		) + "." + base;
		all_prefixed_variables.insert(var.name);
	}
	void add_namespace_prefix(NodeReference& ref) const {
		if (all_prefixed_variables.empty()) return;
		/// important because we do not want to add namespace prefix to references that are
		/// already part of an access chain (e.g. "A.x") but only to the first member ("A")
		/// issue #21
		if (ref.in_access_chain()) return;
		// assume that the reference is only the base and has not already been prefixed
		// Try to find the declaration level for the *basename* (shadowing-aware).
		// A dotted reference is prefixed through its leading segment: <inst.idx> and
		// <Envelope.MAX> both hang off a declaration named by their first segment. The
		// full name is still matched first, because a <family> declared inside a
		// namespace registers a dotted name of its own (<voice.index>).
		const auto leading_segment = ref.name.substr(0, ref.name.find('.'));
		for (int lvl = static_cast<int>(m_namespace_variables.size()) - 1; lvl >= 0; --lvl) {
			const auto& scope = m_namespace_variables[lvl];
			if (!scope.contains(ref.name) && !scope.contains(leading_segment)) continue;

			// Build the exact prefix up to the matched level (0..lvl)
			std::string needed;
			for (size_t i = 0; i <= lvl; ++i) {
				if (!needed.empty()) needed += '.';
				needed += prefix[i].token.val;
				ref.add_prefix(prefix[i]);
			}

			ref.name = needed + "." + ref.name;
			return;
		}

		// Not a variable of an enclosing scope: the leading segment may name a nested
		// namespace instead, written relative to the current one (<mixer.volume> inside
		// <audio> means <audio.mixer.volume>). Walk the enclosing prefix from innermost
		// outward and take the first block whose full path exists, so a nested block
		// shadows a same-named one elsewhere.
		auto splits = StringUtils::split(ref.name, '.');
		for (int lvl = static_cast<int>(prefix.size()); lvl >= 0; --lvl) {
			std::vector<std::string> candidate;
			candidate.reserve(lvl + 1);
			for (int i = 0; i < lvl; ++i) candidate.push_back(prefix[i].token.val);
			candidate.push_back(splits[0]);
			if (!namespace_data.contains(StringUtils::join(candidate, "."))) continue;

			for (int i = 0; i < lvl; ++i) ref.add_prefix(prefix[i]);
			splits.insert(splits.begin(), candidate.begin(), candidate.end() - 1);
			ref.name = StringUtils::join(splits, ".");
			return;
		}

		// Not found in any namespace scope -> leave as-is (could be global/other namespace).
		// Resolver can diagnose unresolved identifiers later if needed.
	}
public:
	explicit DesugarNamespace(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST * visit(NodeNamespace& node) override {
		m_namespace_variables.emplace_back();
		prefix.push_back({node.prefix, NodePrefix::PrefixKind::Namespace});
		node.members->accept(*this);
		for(const auto & m: node.function_definitions) {
			m->accept(*this);
		}
		m_namespace_variables.pop_back();
		prefix.pop_back();
		return &node;
	}

	// function parameter do not need namespace prefix
	NodeAST * visit(NodeFunctionParam& node) override {
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST * visit(NodeVariable& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodePointer& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodePointerRef& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST* visit(NodeFunctionHeader& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST* visit(NodeFunctionHeaderRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeArray& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeNDArray& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeNDArrayRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeList& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeListRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeConst& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeStruct& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	// ---------- helpers ----------



	/// Dot-joined path of a prefix stack, used as the identity of a namespace block.
	static std::string path_string(const std::vector<NodePrefix::PrefixSegment>& prefixes) {
		return StringUtils::join_apply(
			prefixes,
			[](const NodePrefix::PrefixSegment& segment) { return segment.token.val; },
			"."
		);
	}

	// Return last identifier (after the last '.')
	static std::string basename_of(const std::string& name, const std::vector<NodePrefix::PrefixSegment>& prefixes) {
		auto splits = StringUtils::split(name, '.');
		if (splits.empty()) return name;
		if (splits.size() == 1) return name;

		// check if the first segment matches any of the prefixes
		int nesting_lvl = -1;
		for (int i = 0; i< prefixes.size(); ++i) {
			if (splits[0] == prefixes[i].token.val) {
				nesting_lvl = i;
				break;
			}
		}

		if (nesting_lvl == -1) return name; // no prefix match, return as-is

		size_t p = nesting_lvl;
		while (!splits.empty() && p < prefixes.size() && splits.front() == prefixes[p].token.val) {
			// pop front (O(n))
			splits.erase(splits.begin());
			++p;
		}

		return StringUtils::join(splits, ".");
	}

};
