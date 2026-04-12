//
// Created by Mathias Vatter on 10.07.25.
//

#pragma once


#include <ranges>

#include "ASTDesugaring.h"
#include "../AST/ASTVisitor/ASTNoVisitor.h"

struct NamespaceData {
	std::unordered_set<std::string> variables; // all variables declared in this namespace
	std::vector<std::string> path; // path to the namespace, e.g. "a.b.c" for namespace "c" in "a.b"
};

/**
 * Prepending prefix to variables and references that were declared in namespace
 */
class DesugarNamespace final : public ASTDesugaring {
	std::vector<std::string> prefix;
	std::vector<std::unordered_set<std::string>> m_namespace_variables;
	// collects all basenames of variables by the namespace prefix
	std::unordered_map<std::string, std::unique_ptr<NamespaceData>> namespace_data;
	std::unordered_set<std::string> all_prefixed_variables; // all variables that have been prefixed with a namespace

	void add_namespace_prefix(NodeDataStructure& var) {
		if (m_namespace_variables.empty()) return;

		// Register unqualified name in the current (innermost) scope
		const std::string base = basename_of(var.name, prefix);
		m_namespace_variables.back().insert(base);

		auto it = namespace_data.find(prefix.back());
		if (it == namespace_data.end()) {
			// create new namespace data for this prefix
			auto data = std::make_unique<NamespaceData>();
			data->variables.insert(base);
			data->path = prefix;
			namespace_data[prefix.back()] = std::move(data);
		} else {
			it->second->variables.insert(base);
		}

		var.name = StringUtils::join(prefix, '.') + "." + base;
		all_prefixed_variables.insert(var.name);
	}
	void add_namespace_prefix(NodeReference& ref) const {
		if (all_prefixed_variables.empty()) return;
		if (in_access_chain(ref)) return;
		// assume that the reference is only the base and has not already been prefixed
		// Try to find the declaration level for the *basename* (shadowing-aware).
		for (int lvl = static_cast<int>(m_namespace_variables.size()) - 1; lvl >= 0; --lvl) {
			const auto& scope = m_namespace_variables[lvl];
			if (!scope.contains(ref.name)) continue;

			// Build the exact prefix up to the matched level (0..lvl)
			std::string needed;
			for (size_t i = 0; i <= lvl; ++i) {
				if (!needed.empty()) needed += '.';
				needed += prefix[i];
			}

			ref.name = needed + "." + ref.name;
			return;
		}

		// If not found in any namespace scope, we need to check if the first segment of the name
		// matches any of the previous prefixes that might have been nested.
		auto splits = StringUtils::split(ref.name, '.');
		// check if the first segment matches any of the previous prefixes that might have been nested
		auto it = namespace_data.find(splits[0]);
		if (it != namespace_data.end()) {
			// find this prefix (splits[0]) in the namespaceData path
			auto result = std::ranges::find_if(it->second->path,
			   [&](const std::string& var) -> bool {
				   return var == splits[0];
			   }
			);
			if (result != it->second->path.end()) {
				// merge without removing the first element
				splits.insert(splits.begin(), it->second->path.begin(), result);
				ref.name = StringUtils::join(splits, '.');
			}
		}

		// Not found in any namespace scope -> leave as-is (could be global/other namespace).
		// Resolver can diagnose unresolved identifiers later if needed.
	}
public:
	explicit DesugarNamespace(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST * visit(NodeNamespace& node) override {
		m_namespace_variables.emplace_back();
		prefix.push_back(node.prefix);
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

	// ---------- helpers ----------

	/// returns true if ref node is within access chain but not first member
	/// important because we do not want to add namespace prefix to references that are
	/// already part of an access chain (e.g. "A.x") but only to the first member ("A")
	/// issue #21
	static bool in_access_chain(const NodeReference& node) {
		if (node.parent) {
			if (const auto chain = node.parent->cast<NodeAccessChain>()) {
				if (chain->member(0).get() == &node) {
					return false;
				} else {
					return true;
				}
			}
		}
		return false;
	}

	// Return last identifier (after the last '.')
	static std::string basename_of(const std::string& name, const std::vector<std::string>& prefixes) {
		auto splits = StringUtils::split(name, '.');
		if (splits.empty()) return name;
		if (splits.size() == 1) return name;

		// check if the first segment matches any of the prefixes
		int nesting_lvl = -1;
		for (int i = 0; i< prefixes.size(); ++i) {
			if (splits[0] == prefixes[i]) {
				nesting_lvl = i;
				break;
			}
		}

		if (nesting_lvl == -1) return name; // no prefix match, return as-is

		size_t p = nesting_lvl;
		while (!splits.empty() && p < prefixes.size() && splits.front() == prefixes[p]) {
			// pop front (O(n))
			splits.erase(splits.begin());
			++p;
		}

		return StringUtils::join(splits, '.');
	}

	// // English: Given the current full prefix chain (e.g. ["A","B"]) and a possibly
	// // partially qualified name (e.g. "B.x"), prepend the *missing outer head*
	// // so that it becomes fully-qualified inside this nesting ("A.B.x").
	// // If the name already starts with the entire chain ("A.B.x"), it's left unchanged.
	// // If the name starts with a different root ("C.x"), it's treated as absolute and left unchanged.
	// static std::string complete_outer_prefix(const std::vector<std::string>& chain,
	//                                          const std::string& name) {
	//     if (chain.empty()) return name;
	//
	//     auto nameSegs = split_segments(name);
	//     if (nameSegs.empty()) return name;
	//
	//     // Try to match the *longest* suffix of 'chain' against the *beginning* of 'nameSegs'.
	//     // Example: chain=["A","B"], name="B.x"  => match suffix ["B"] (k=1) -> prepend ["A"].
	//     //          chain=["A","B"], name="A.B.x" => match suffix ["A","B"] (k=0) -> prepend [].
	//     //          chain=["A","B"], name="C.x"   => no match -> leave unchanged.
	//     for (size_t k = 0; k < chain.size(); ++k) {
	//         const size_t suffixLen = chain.size() - k;            // length of chain[k..end]
	//         if (nameSegs.size() < suffixLen) continue;
	//
	//         bool matches = true;
	//         for (size_t i = 0; i < suffixLen; ++i) {
	//             if (nameSegs[i] != chain[k + i]) { matches = false; break; }
	//         }
	//         if (!matches) continue;
	//
	//         // Build completed = head(chain[0..k-1]) + nameSegs
	//         if (k == 0) {
	//             // Already has the full chain as prefix -> unchanged
	//             return name;
	//         }
	//         std::vector<std::string> completed;
	//         completed.reserve(chain.size() + nameSegs.size());
	//         // missing head
	//         for (size_t i = 0; i < k; ++i) completed.push_back(chain[i]);
	//         // existing name
	//         completed.insert(completed.end(), nameSegs.begin(), nameSegs.end());
	//         return join_segments(completed);
	//     }
	//
	//     // No suffix of current chain matches the start of 'name' -> treat as absolute; leave unchanged.
	//     return name;
	// }


};
//
// class UnnestNamespaces final : public ASTNoVisitor {
// 	std::string m_prefixes;
// 	void add_prefix(const std::string& prefix) {
// 		if (!m_prefixes.empty()) {
// 			m_prefixes += ".";
// 		}
// 		m_prefixes += prefix;
// 	}
// 	std::stack<NodeNamespace*> m_stack;
// public:
// 	explicit UnnestNamespaces(NodeProgram* program) {
// 		m_program = program;
// 	}
//
// 	NodeAST* unnest(NodeNamespace& node) {
// 		m_prefixes = "";
// 		return node.accept(*this);
// 	}
//
// private:
// 	NodeAST* visit(NodeNamespace& node) override {
// 		add_prefix(node.prefix);
// 		node.prefix = m_prefixes;
// 		node.members->accept(*this);
// 		return &node;
// 	}
//
// 	NodeAST* visit(NodeStatement &node) override {
// 		if (auto ns = node.statement->cast<NodeNamespace>()) {
// 			node.statement->accept(*this);
// 			m_program->namespaces.push_back(unique_ptr_cast<NodeNamespace>(std::move(node.statement)));
// 			node.statement = std::make_unique<NodeDeadCode>(node.tok);
// 		}
// 		return &node;
// 	}
//
// 	NodeAST* visit(NodeBlock &node) override {
// 		return ASTVisitor::visit(node);
// 	}
//
//
// };
