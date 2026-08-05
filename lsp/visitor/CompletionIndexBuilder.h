//
// Created for LSP completion support.
//

#pragma once

#include <string>
#include <vector>

#include "../../cksp/ASTVisitor/ASTVisitor.h"
#include "../../cksp/ASTNodes/ASTReferences.h"
#include "../../cksp/Source/CompletionIndex.h"

/**
 * Harvests qualifier containers and their members into a CompletionIndex.
 *
 * Two passes, mirroring ReferenceIndexBuilder. The Scopes pass runs before
 * desugaring, where the namespace/family/const blocks still exist and carry the
 * source range of their body; those ranges let a shortened qualifier be resolved
 * against the block the cursor sits in.
 *
 * The Members pass runs after desugaring, where every declaration carries its
 * complete qualifier path in NodeDataStructure::prefix and its qualified name.
 * That makes the member harvest a grouping over declarations rather than a scope
 * analysis: the container is the prefix path, the member is the basename.
 *
 * Struct statics take a third route: they are not prefixed declarations but
 * members of a NodeStruct, addressed through the struct name (`Foo.BAR`) while
 * being stored with the OBJ_DELIMITER internally (`Foo::BAR`).
 */
class CompletionIndexBuilder final : public ASTVisitor {
public:
	enum class Pass {
		/// Before desugaring, while the namespace/family/const blocks still exist
		/// and carry the source range of their body.
		Scopes,
		/// After desugaring, when every declaration carries its full prefix path.
		Members
	};

private:
	CompletionIndex& m_index;
	Pass m_pass;
	std::vector<std::string> m_scope_path;
	/// Nesting depth inside function bodies; declarations there are not members.
	size_t m_function_depth = 0;
	/// Body of the function currently being visited; invalid at global scope.
	SourceRange m_function_scope{};
	std::string m_function_scope_file;

	/// Records the block body so a shortened qualifier typed inside it can be
	/// resolved against the enclosing path.
	template<typename Node>
	NodeAST* record_scope(Node& node, const Token& name) {
		m_scope_path.push_back(name.val);
		if (m_pass == Pass::Scopes && !node.tok.file.empty()) {
			m_index.add_scope(node.tok.file, node.range, m_scope_path);
		}
		ASTVisitor::visit(node);
		m_scope_path.pop_back();
		return &node;
	}

	static std::string detail_of(const NodeAST& node) {
		return node.ty ? node.ty->to_string() : std::string{};
	}

	/// The declared name minus its qualifier path, i.e. what the user types
	/// after the dot.
	static std::string basename_of(const std::string& name) {
		const auto object_member = name.rfind(OBJ_DELIMITER);
		if (object_member != std::string::npos) {
			return name.substr(object_member + OBJ_DELIMITER.size());
		}
		const auto dot = name.rfind('.');
		return dot == std::string::npos ? name : name.substr(dot + 1);
	}

	static CompletionKind kind_of_segment(const NodePrefix::PrefixKind kind) {
		switch (kind) {
			case NodePrefix::PrefixKind::Const: return CompletionKind::Constant;
			case NodePrefix::PrefixKind::Family: return CompletionKind::Field;
			case NodePrefix::PrefixKind::Namespace: return CompletionKind::Module;
		}
		return CompletionKind::Module;
	}

	static CompletionKind kind_of_member(const NodeDataStructure& node) {
		if (node.prefix->prefixes.empty()) return CompletionKind::Variable;
		switch (node.prefix->prefixes.back().kind) {
			case NodePrefix::PrefixKind::Const: return CompletionKind::Constant;
			case NodePrefix::PrefixKind::Family: return CompletionKind::Field;
			case NodePrefix::PrefixKind::Namespace: return CompletionKind::Variable;
		}
		return CompletionKind::Variable;
	}

	static std::vector<std::string> path_of(const NodeDataStructure& node) {
		std::vector<std::string> path;
		path.reserve(node.prefix->prefixes.size());
		for (const auto& segment : node.prefix->prefixes) {
			path.push_back(segment.token.val);
		}
		return path;
	}

	/// Registers the containers along a qualifier path as members of their
	/// parent, so `audio.` offers the nested `mixer` and not just its variables.
	void record_nesting(const NodeDataStructure& node) const {
		const auto& segments = node.prefix->prefixes;
		std::vector<std::string> parent;
		for (const auto& segment : segments) {
			if (!parent.empty()) {
				m_index.add(parent, {segment.token.val, {}, {}, kind_of_segment(segment.kind)});
			}
			parent.push_back(segment.token.val);
		}
	}

	void record(const NodeDataStructure& node, const CompletionKind kind) const {
		if (m_pass != Pass::Members || m_function_depth > 0) return;
		if (node.prefix->prefixes.empty()) return;
		record_nesting(node);
		const auto path = path_of(node);
		m_index.set_container_kind(
			CompletionIndex::join(path), kind_of_segment(node.prefix->prefixes.back().kind));
		m_index.add(path, {basename_of(node.name), {}, detail_of(node), kind});
	}

	void record_declaration(NodeDataStructure& node) const {
		record(node, kind_of_member(node));
		record_named_declaration(node);
	}

	/// "(amount: int, target: int)".
	///
	/// Parameter names come from the declaration token, never from the name:
	/// UniqueParameterNamesProvider uniquifies the names right before this harvest runs,
	/// which would surface <amount0> in the editor. The token keeps the source spelling.
	///
	/// Read tok.val rather than get_token_string(): the latter reconstructs a declaration
	/// from the *name* for composite structures (NodeArray returns "name[size]"), so it
	/// carries the rename right back in.
	static std::string parameters_of(const NodeFunctionHeader& header) {
		std::string parameters = "(";
		for (size_t i = 0; i < header.params.size(); ++i) {
			if (i) parameters += ", ";
			const auto& parameter = header.params[i];
			if (!parameter || !parameter->variable) continue;
			parameters += parameter->variable->tok.val;
			if (const auto* type = parameter->variable->ty; type && !is_unknown(type)) {
				parameters += ": " + type->to_string();
			}
		}
		return parameters + ")";
	}

	/// "function fade(amount: int, target: int): int".
	static std::string signature_of(const NodeFunctionHeader& header, const bool is_static) {
		std::string signature = is_static ? "static function " : "function ";
		signature += basename_of(header.name) + parameters_of(header);
		// The header's own type is a FunctionType; its return type is Unknown for a
		// function that returns nothing, which must not be printed.
		if (const auto* function_type = header.ty ? header.ty->cast<FunctionType>() : nullptr) {
			if (const auto* return_type = function_type->get_return_type();
				return_type && !is_unknown(return_type)) {
				signature += ": " + return_type->to_string();
			}
		}
		return signature;
	}

	static bool is_unknown(const Type* type) {
		return !type || type->to_string() == "unknown";
	}

	/// Struct a declaration is an instance of. Arrays of structs resolve to their
	/// element type, so <zones[0].> can walk on through it.
	static std::string object_type_of(const Type* type) {
		if (!type) return {};
		if (type->get_type_kind() == TypeKind::Object) return type->to_string();
		const auto* element = type->get_element_type();
		if (element && element != type && element->get_type_kind() == TypeKind::Object) {
			return element->to_string();
		}
		return {};
	}

	/// Every declaration the user can name, with the only scope CKSP actually hides
	/// things in: a function body. Declarations in callbacks are hoisted to the global
	/// scope, so they stay unscoped here.
	void record_named_declaration(const NodeDataStructure& node) const {
		if (m_pass != Pass::Members || node.name.empty()) return;
		// Synthesized declarations have no source token to offer.
		if (node.tok.val.empty() || node.tok.file.empty()) return;
		if (node.name == "self" || node.tok.val == "self") return;

		std::string name = node.name;
		if (basename_of(name) != node.tok.val) {
			// The compiler renamed this one - a uniquified parameter, or machinery of a
			// generated method. Only a declaration inside a real function body stays
			// nameable, and then under the spelling the source actually uses.
			if (!m_function_scope.is_valid()) return;
			name = node.tok.val;
		}
		m_index.add_declaration({
			.name = std::move(name),
			.object_type = object_type_of(node.ty),
			.detail = detail_of(node),
			.kind = kind_of_member(node),
			.file = m_function_scope_file,
			.scope = m_function_scope,
		});
	}

public:
	explicit CompletionIndexBuilder(CompletionIndex& index, const Pass pass)
		: m_index(index), m_pass(pass) {}

	NodeAST* visit(NodeNamespace& node) override { return record_scope(node, node.prefix); }
	NodeAST* visit(NodeFamily& node) override { return record_scope(node, node.prefix); }
	NodeAST* visit(NodeConst& node) override { return record_scope(node, node.const_prefix); }

	NodeAST* visit(NodeVariable& node) override {
		record_declaration(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodePointer& node) override {
		record_declaration(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeArray& node) override {
		record_declaration(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeNDArray& node) override {
		record_declaration(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeList& node) override {
		record_declaration(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionHeader& node) override {
		if (m_pass == Pass::Members && !node.prefix->prefixes.empty()) {
			record_nesting(node);
			m_index.add(path_of(node), {
				basename_of(node.name),
				parameters_of(node),
				signature_of(node, false),
				CompletionKind::Function,
			});
		}
		// Also nameable without a qualifier. Methods carry the OBJ_DELIMITER and are
		// filtered out there; generated helpers have no source file.
		if (m_pass == Pass::Members && !node.tok.file.empty()) {
			m_index.add_declaration({
				.name = node.name,
				.parameters = parameters_of(node),
				.detail = signature_of(node, false),
				.kind = CompletionKind::Function,
			});
		}
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeStruct& node) override {
		// The struct as a member of its enclosing namespace.
		record(node, CompletionKind::Struct);
		return ASTVisitor::visit(node);
	}

	/// A function declared in a namespace is a member of it, but its body is not: the
	/// namespace desugaring prefixes function-local declarations just like real members
	/// (<PathUtils._id>), and those are unreachable through the qualifier.
	NodeAST* visit(NodeFunctionDefinition& node) override {
		if (node.header) node.header->accept(*this);
		const auto outer_scope = m_function_scope;
		const auto outer_file = m_function_scope_file;
		m_function_scope = node.range;
		m_function_scope_file = node.tok.file;
		++m_function_depth;
		ASTVisitor::visit(node);
		--m_function_depth;
		m_function_scope = outer_scope;
		m_function_scope_file = outer_file;
		return &node;
	}

	NodeAST* visit(NodeProgram& node) override {
		ASTVisitor::visit(node);
		if (m_pass == Pass::Members) collect_struct_members(node);
		return &node;
	}

	/// `Foo.` names the struct, not an instance, so only <static> members and
	/// <static> methods are reachable through it; an instance reaches exactly the
	/// others. Both come from the same tables, split by the static flag.
	void collect_struct_members(const NodeProgram& program) const {
		for (const auto* definition : program.struct_definitions) {
			if (!definition) continue;
			const auto container = definition->name;
			if (container.empty()) continue;
			m_index.add_declaration({
				.name = container,
				.detail = "struct " + basename_of(container),
				.kind = CompletionKind::Struct,
			});

			for (const auto& [name, member] : definition->member_table) {
				const auto declaration = member.lock();
				if (!declaration) continue;
				// Struct desugaring prepends a synthetic <self> member; it is machinery,
				// not something the user can complete to.
				if (declaration->name == "self" || basename_of(declaration->name) == "self") {
					continue;
				}
				CompletionMember item{
					basename_of(declaration->name),
					{},
					detail_of(*declaration),
					CompletionKind::Constant,
					object_type_of(declaration->ty),
				};
				if (declaration->is_shared_member()) {
					m_index.add(container, std::move(item));
				} else {
					item.kind = CompletionKind::Field;
					m_index.add_type_member(container, std::move(item));
				}
			}

			for (const auto& method : definition->methods) {
				if (!method || !method->header) continue;
				const auto name = basename_of(method->header->name);
				// Generated lifecycle methods are not part of the surface.
				if (name == NodeStruct::CONSTRUCTOR || name == NodeStruct::DESTRUCTOR
					|| name == "__repr__") {
					continue;
				}
				CompletionMember item{
					name,
					parameters_of(*method->header),
					signature_of(*method->header, method->is_static),
					CompletionKind::Method,
				};
				if (method->is_static) {
					m_index.add(container, std::move(item));
				} else {
					m_index.add_type_member(container, std::move(item));
					// <self.> inside this method resolves to the struct.
					if (!method->tok.file.empty()) {
						m_index.add_self_scope(method->tok.file, method->range, container);
					}
				}
			}
		}
	}
};
