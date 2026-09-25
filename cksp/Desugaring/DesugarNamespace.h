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
	/// The struct names each open block declares, for the type annotations that name them.
	std::vector<std::unordered_set<std::string>> m_namespace_structs;
	/// Member blocks and methods of the structs of the block being read, taken once it is
	/// complete - a struct names a sibling that may stand below it.
	std::vector<NodeBlock*> m_struct_bodies;
	std::vector<NodeFunctionDefinition*> m_struct_methods;
	/// Set while one of them is walked, where a declaration is never the namespace's.
	bool m_in_struct_body = false;

	void add_namespace_prefix(NodeDataStructure& var) {
		if (m_namespace_variables.empty()) return;
		// Nothing a method declares belongs to the namespace: its locals are its own, and its
		// members are the struct's. Only the references in there are of interest.
		if (m_in_struct_body) return;

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
	/// The object type a name inside this block stands for: <Slot> written in <namespace engine>
	/// is <engine.Slot>.
	///
	/// A type annotation is resolved where it is written, which is before the block it stands in
	/// has been read, so what it was given is the type of the unprefixed name. Re-pointed here to
	/// the very type the struct itself is given, so the two are one type and not two that happen
	/// to end in the same word.
	[[nodiscard]] Type* namespaced_type(Type* ty) const {
		if (const auto composite = ty ? ty->cast<CompositeType>() : nullptr) {
			const auto element = namespaced_type(composite->get_element_type());
			return element == composite->get_element_type()
				? ty
				: TypeRegistry::add_composite_type(
					composite->get_compound_type(), element, composite->get_dimensions());
		}
		// A function's own type holds the annotations of its signature.
		if (const auto function = ty ? ty->cast<FunctionType>() : nullptr) {
			std::vector<Type*> params;
			params.reserve(function->get_params().size());
			bool changed = false;
			for (const auto param : function->get_params()) {
				params.push_back(namespaced_type(param));
				changed |= params.back() != param;
			}
			const auto return_type = namespaced_type(function->get_return_type());
			changed |= return_type != function->get_return_type();
			return changed ? TypeRegistry::add_function_type(params, return_type) : ty;
		}
		const auto object = ty ? ty->cast<ObjectType>() : nullptr;
		if (!object) return ty;

		const auto& name = object->to_string();
		for (int lvl = static_cast<int>(m_namespace_structs.size()) - 1; lvl >= 0; --lvl) {
			if (!m_namespace_structs[lvl].contains(name)) continue;
			std::string prefixed;
			for (int i = 0; i <= lvl; ++i) prefixed += prefix[i].token.val + ".";
			return TypeRegistry::add_object_type(prefixed + name);
		}
		return ty;
	}

	/// Re-points what a node was annotated with, and what the editor links its name to.
	void resolve_namespaced_type(NodeAST& node) const {
		if (m_namespace_structs.empty()) return;
		node.ty = namespaced_type(node.ty);
		for (auto& reference : node.type_references) {
			reference.type = namespaced_type(reference.type);
		}
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
		m_namespace_structs.emplace_back();
		prefix.push_back({node.prefix, NodePrefix::PrefixKind::Namespace});
		auto outer_bodies = std::move(m_struct_bodies);
		auto outer_methods = std::move(m_struct_methods);
		m_struct_bodies.clear();
		m_struct_methods.clear();
		node.members->accept(*this);
		for(const auto & m: node.function_definitions) {
			m->accept(*this);
		}
		// The method bodies come last, when every name the block declares is registered: a
		// struct names a sibling that may be written below it, and a namespace is one scope.
		const auto bodies = std::move(m_struct_bodies);
		const auto methods = std::move(m_struct_methods);
		m_struct_bodies = std::move(outer_bodies);
		m_struct_methods = std::move(outer_methods);
		m_in_struct_body = true;
		for (const auto& body : bodies) {
			body->accept(*this);
		}
		for (const auto method : methods) {
			// The signature is resolved by hand rather than visited: a method keeps its own
			// name, which the struct prefixes later, and only the types it names are the
			// namespace's business.
			resolve_namespaced_type(*method);
			if (method->header) {
				resolve_namespaced_type(*method->header);
				for (const auto& param : method->header->params) {
					if (param->variable) resolve_namespaced_type(*param->variable);
				}
			}
			if (method->body) method->body->accept(*this);
		}
		m_in_struct_body = false;
		m_namespace_variables.pop_back();
		m_namespace_structs.pop_back();
		prefix.pop_back();
		return &node;
	}

	// function parameter do not need namespace prefix
	NodeAST * visit(NodeFunctionParam& node) override {
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST * visit(NodeVariable& node) override {
		resolve_namespaced_type(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		resolve_namespaced_type(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodePointer& node) override {
		resolve_namespaced_type(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodePointerRef& node) override {
		resolve_namespaced_type(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST* visit(NodeFunctionHeader& node) override {
		ASTVisitor::visit(node);
		resolve_namespaced_type(node);
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
		resolve_namespaced_type(node);
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
		resolve_namespaced_type(node);
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
		if (!m_namespace_structs.empty()) {
			m_namespace_structs.back().insert(basename_of(node.name, prefix));
		}
		add_namespace_prefix(node);
		// A method names its own members through <self>, and its parameters and locals are its
		// own - but a sibling struct, a function or a variable of the enclosing block it names
		// unqualified is the one declared there. Its body is therefore walked like any other
		// code of the namespace, once the block has been read to its end.
		if (node.members) m_struct_bodies.push_back(node.members.get());
		for (const auto& method : node.methods) {
			m_struct_methods.push_back(method.get());
		}
		if (node.constructor) m_struct_methods.push_back(node.constructor.get());
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
