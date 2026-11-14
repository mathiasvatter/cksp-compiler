//
// Created for LSP completion support.
//

#pragma once

#include <string>
#include <vector>

#include "../../cksp/ASTVisitor/ASTVisitor.h"
#include "../../cksp/ASTNodes/ASTReferences.h"
#include "../../cksp/Source/CompletionIndex.h"
#include "DeclarationFormat.h"

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
	/// Innermost construct that hides a declaration: the function, or the loop or
	/// branch inside it the cursor would have to be in to still see the name.
	SourceRange m_declaration_scope{};

	/// Records the block body so a shortened qualifier typed inside it can be
	/// resolved against the enclosing path.
	template<typename Node>
	NodeAST* record_scope(Node& node, const Token& name) {
		m_scope_path.push_back(name.val);
		if (m_pass == Pass::Scopes && !node.tok.file().empty()) {
			m_index.add_scope(node.tok.file(), node.range, m_scope_path);
		}
		ASTVisitor::visit(node);
		m_scope_path.pop_back();
		return &node;
	}

	static std::string detail_of(const NodeAST& node) {
		return node.ty ? node.ty->to_string() : std::string{};
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
		m_index.add(path, {declaration_format::basename_of(node.name), {}, detail_of(node), kind});
	}

	void record_declaration(NodeDataStructure& node) const {
		record(node, kind_of_member(node));
		record_named_declaration(node);
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

	/// Every declaration the user can name, with the construct it is hidden in: the
	/// enclosing function, or the loop or branch inside it that ends the name's life.
	/// Declarations in callbacks are hoisted to the global scope, so they stay unscoped
	/// here no matter how deeply they are nested.
	void record_named_declaration(const NodeDataStructure& node) const {
		if (m_pass != Pass::Members || node.name.empty()) return;
		// Synthesized declarations have no source token to offer.
		if (node.tok.val.empty() || node.tok.file().empty()) return;
		if (node.name == NodeStruct::SELF || node.tok.val == NodeStruct::SELF) return;

		std::string name = node.name;
		// A name may equal the token outright when it carries dots of its own
		// (<declare nks.counter>), or differ from it only by the qualifier the
		// namespace desugaring prepended.
		if (name != node.tok.val && declaration_format::basename_of(name) != node.tok.val) {
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
			.scope = m_declaration_scope,
		});
	}

	/**
	 * Visits a construct whose body is a scope of its own.
	 *
	 * A NodeBlock is a scope unless its parent is a statement (NodeBlock::determine_scope),
	 * so every loop and branch body hides the declarations written in it - but only inside
	 * a function. A declaration in a callback is hoisted to the global scope and outlives
	 * the block it stands in, which is why the narrowing is gated on the function depth.
	 *
	 * The construct's own range stands in for the body's: a NodeBlock carries no source
	 * range. For <if>/<else> that makes both branches one scope, which offers a name
	 * across the <else> it does not reach - it never hides one that is in scope.
	 */
	template<typename Node>
	NodeAST* visit_nested_scope(Node& node) {
		// Qualified: NodeForEach::range is the iterable being walked, not a source range.
		const auto& source_range = static_cast<const NodeAST&>(node).range;
		const bool narrows = m_function_depth > 0 && source_range.is_valid();
		const auto outer = m_declaration_scope;
		if (narrows) m_declaration_scope = source_range;
		ASTVisitor::visit(node);
		if (narrows) m_declaration_scope = outer;
		return &node;
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
		const auto parameter_labels = declaration_format::parameter_labels_of(node);
		if (m_pass == Pass::Members && !node.prefix->prefixes.empty()) {
			record_nesting(node);
			m_index.add(path_of(node), {
				.label = declaration_format::basename_of(node.name),
				.parameters = declaration_format::parameters_of(parameter_labels),
				.detail = declaration_format::signature_of(node, false),
				.kind = CompletionKind::Function,
				.parameter_labels = parameter_labels,
			});
		}
		// Also nameable without a qualifier. Methods carry the OBJ_DELIMITER and are
		// filtered out there; generated helpers have no source file.
		if (m_pass == Pass::Members && !node.tok.file().empty()) {
			m_index.add_declaration({
				.name = node.name,
				.parameters = declaration_format::parameters_of(parameter_labels),
				.detail = declaration_format::signature_of(node, false),
				.kind = CompletionKind::Function,
				.parameter_labels = parameter_labels,
			});
		}
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeStruct& node) override {
		// The struct as a member of its enclosing namespace.
		record(node, CompletionKind::Struct);
		return ASTVisitor::visit(node);
	}

	/// A function declared in a namespace is a member of it, but its body is not: its locals
	/// are never reachable through the qualifier. The namespace desugaring leaves them
	/// unprefixed (#147), so they would not even land in its container.
	///
	/// The header is left to the base traversal below rather than being visited up front:
	/// ASTVisitor::visit() accepts it too, and a parameter harvested by that first visit
	/// carried the scope of whatever encloses the *definition* - none, for a function at
	/// global scope - which offered every parameter of every function everywhere.
	NodeAST* visit(NodeFunctionDefinition& node) override {
		const auto outer_scope = m_function_scope;
		const auto outer_file = m_function_scope_file;
		const auto outer_declaration_scope = m_declaration_scope;
		m_function_scope = node.range;
		m_function_scope_file = node.tok.file();
		// Until a loop or a branch narrows it, the whole body is the scope.
		m_declaration_scope = node.range;
		++m_function_depth;
		ASTVisitor::visit(node);
		--m_function_depth;
		m_function_scope = outer_scope;
		m_function_scope_file = outer_file;
		m_declaration_scope = outer_declaration_scope;
		return &node;
	}

	// Loop and branch bodies are scopes of their own; see visit_nested_scope().
	NodeAST* visit(NodeFor& node) override { return visit_nested_scope(node); }
	NodeAST* visit(NodeForEach& node) override { return visit_nested_scope(node); }
	NodeAST* visit(NodeWhile& node) override { return visit_nested_scope(node); }
	NodeAST* visit(NodeIf& node) override { return visit_nested_scope(node); }
	NodeAST* visit(NodeSelect& node) override { return visit_nested_scope(node); }

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
				.detail = "struct " + declaration_format::basename_of(container),
				.kind = CompletionKind::Struct,
			});

			for (const auto& [name, member] : definition->member_table) {
				const auto declaration = member.lock();
				if (!declaration) continue;
				// Struct desugaring prepends a synthetic <self> member; it is machinery,
				// not something the user can complete to.
				if (declaration->name == NodeStruct::SELF || declaration_format::basename_of(declaration->name) == NodeStruct::SELF) {
					continue;
				}
				CompletionMember item{
					.label = declaration_format::basename_of(declaration->name),
					.detail = detail_of(*declaration),
					.object_type = object_type_of(declaration->ty),
				};
				if (declaration->is_shared_member()) {
					item.kind = CompletionKind::Constant;
					item.category = "static const";
					m_index.add(container, std::move(item));
				} else {
					item.kind = CompletionKind::Field;
					item.category = "member";
					m_index.add_type_member(container, std::move(item));
				}
			}

			for (const auto& method : definition->methods) {
				if (!method || !method->header) continue;
				const auto name = declaration_format::basename_of(method->header->name);
				// <self.> inside this method resolves to the struct. Recorded before the
				// surface filter below: a hand written <__init__> is not offered as a
				// member, but its body is source the user writes <self.> in like any
				// other. A generated one carries the struct token and no range, which
				// add_self_scope drops.
				if (!method->is_static && !method->tok.file().empty()) {
					m_index.add_self_scope(method->tok.file(), method->range, container);
				}
				// Generated lifecycle methods are not part of the surface.
				if (name == NodeStruct::CONSTRUCTOR || name == NodeStruct::DESTRUCTOR
					|| name == NodeStruct::REPRESENTOR) {
					continue;
				}
				const bool omit_receiver = !method->is_static;
				const auto parameter_labels = declaration_format::parameter_labels_of(*method->header, omit_receiver);
				CompletionMember item{
					.label = name,
					.parameters = declaration_format::parameters_of(parameter_labels),
					.detail = declaration_format::signature_of(*method->header, method->is_static, omit_receiver),
					.kind = CompletionKind::Method,
					.category = method->is_static ? "static function" : "method",
					.parameter_labels = parameter_labels,
				};
				if (method->is_static) {
					m_index.add(container, std::move(item));
				} else {
					m_index.add_type_member(container, std::move(item));
				}
			}
		}
	}
};
