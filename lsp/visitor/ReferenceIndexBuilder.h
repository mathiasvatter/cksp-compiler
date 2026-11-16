//
// Created by Claude for go-to-definition support.
//

#pragma once

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../cksp/ASTVisitor/ASTVisitor.h"
#include "../../cksp/ASTNodes/ASTReferences.h"
#include "../../cksp/Source/ReferenceIndex.h"
#include "../../cksp/Source/SourceProvider.h"
#include "DeclarationFormat.h"

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
	/// Only the first reference harvest renders hovers. The second one runs after
	/// ASTCollectLowerings, when struct members are mangled arrays and objects plain ints.
	bool m_record_hovers = true;
	mutable std::unordered_map<std::string, std::shared_ptr<const std::string>> m_source_cache;

	struct Symbol {
		SymbolKind kind;
		unsigned modifiers = 0;
	};

	static Symbol symbol_of(const NodePrefix::PrefixKind kind) {
		return {kind == NodePrefix::PrefixKind::Const ? SymbolKind::Enum : SymbolKind::Namespace};
	}

	static Symbol symbol_of(NodeFunctionHeader& header) {
		auto* definition = header.parent ? header.parent->cast<NodeFunctionDefinition>() : nullptr;
		if (!definition) return {SymbolKind::Function};
		// Not is_method(): it looks for a parameter named <self>, which the reference pass
		// meets already renamed. Every function of a struct body is a method.
		if (!definition->parent || !definition->parent->cast<NodeStruct>()) return {SymbolKind::Function};
		return {SymbolKind::Method, definition->is_static ? SymbolToken::Static : 0u};
	}

	/// What a declaration is as a symbol, or nothing for one an editor should not colour.
	static std::optional<Symbol> symbol_of(NodeAST& target) {
		if (auto* header = target.cast<NodeFunctionHeader>()) return symbol_of(*header);
		if (target.cast<NodeStruct>()) return Symbol{SymbolKind::Struct};
		if (target.cast<NodeNamespace>() || target.cast<NodeFamily>()) return Symbol{SymbolKind::Namespace};
		if (target.cast<NodeConst>()) return Symbol{SymbolKind::Enum};
		auto* data = dynamic_cast<NodeDataStructure*>(&target);
		if (!data || data->kind == NodeDataStructure::Kind::Builtin) return std::nullopt;
		// <self> is a keyword to the reader, and the one struct desugaring declares carries
		// the struct name's token.
		if (data->name == NodeStruct::SELF || data->tok.val == NodeStruct::SELF) return std::nullopt;
		const unsigned readonly = data->data_type == DataType::Const ? SymbolToken::Readonly : 0u;
		if (data->is_function_param()) return Symbol{SymbolKind::Parameter, readonly};
		if (data->is_member()) {
			return Symbol{SymbolKind::Property, readonly | (data->is_shared_member() ? SymbolToken::Static : 0u)};
		}
		return Symbol{SymbolKind::Variable, readonly};
	}

	/// The qualifier path a desugared declaration carries, joined: "audio.mixer.".
	static std::string qualifier_of(const NodeDataStructure& node) {
		std::string qualifier;
		for (const auto& segment : node.prefix->prefixes) {
			qualifier += segment.token.val + ".";
		}
		return qualifier;
	}

	/// <declare> and the keywords the declaration was written with: its persistence
	/// (<pers>, <instpers>, <read>) and <const> or <polyphonic>. The hover shows valid CKSP so
	/// the editor's grammar colours it - without <declare> it reads as no statement at all.
	static std::string modifiers_of(const NodeDataStructure& node) {
		std::string modifiers = "declare ";
		if (node.persistence && !node.persistence->val.empty()) modifiers += node.persistence->val + " ";
		if (node.data_type == DataType::Const) modifiers += "const ";
		if (node.data_type == DataType::Polyphonic) modifiers += "polyphonic ";
		return modifiers;
	}

	static std::string typed(std::string name, const NodeAST& node) {
		const auto type = declaration_format::type_of(node);
		return type.empty() ? name : name + ": " + type;
	}

	/// "function audio.fade(amount: int): int", "(method) Loader.update()".
	static std::string hover_of(const NodeFunctionHeader& header) {
		const auto* definition = header.parent ? header.parent->cast<NodeFunctionDefinition>() : nullptr;
		const auto* owner = definition && definition->parent ? definition->parent->cast<NodeStruct>() : nullptr;
		if (owner) {
			const bool is_static = definition->is_static;
			return std::string(is_static ? "(static method) " : "(method) ")
				+ declaration_format::basename_of(owner->name) + "." + header.tok.val
				+ declaration_format::parameters_of(
					declaration_format::parameter_labels_of(header, !is_static))
				+ declaration_format::return_annotation_of(header);
		}
		return "function " + qualifier_of(header) + header.tok.val
			+ declaration_format::parameters_of(declaration_format::parameter_labels_of(header))
			+ declaration_format::return_annotation_of(header);
	}

	/// What hovering a declaration shows, written like TypeScript's quick info: the
	/// construct, the name as it is reached, and the type inference settled on. Empty for
	/// what has no hover.
	static std::string hover_of(NodeAST& target) {
		if (const auto* header = target.cast<NodeFunctionHeader>()) return hover_of(*header);
		if (const auto* definition = target.cast<NodeStruct>()) {
			return "struct " + declaration_format::basename_of(definition->name);
		}
		if (const auto* block = target.cast<NodeNamespace>()) return "namespace " + block->prefix.val;
		if (const auto* block = target.cast<NodeFamily>()) return "family " + block->prefix.val;
		if (const auto* block = target.cast<NodeConst>()) return "const " + block->const_prefix.val;

		const auto* data = dynamic_cast<const NodeDataStructure*>(&target);
		if (!data || data->kind == NodeDataStructure::Kind::Builtin) return {};
		if (data->name == NodeStruct::SELF || data->tok.val == NodeStruct::SELF) return {};
		const auto& name = data->tok.val;
		if (const auto* control = data->parent ? data->parent->cast<NodeUIControl>() : nullptr) {
			return modifiers_of(*data) + control->ui_control_type + " " + qualifier_of(*data) + name;
		}
		if (data->is_function_param()) return "(parameter) " + typed(name, *data);
		if (const auto* owner = data->is_member()) {
			const auto* role = !data->is_shared_member() ? "(member) "
				: data->data_type == DataType::Const ? "(static const) " : "(static) ";
			return std::string(role)
				+ typed(declaration_format::basename_of(owner->name) + "." + name, *data);
		}
		const auto qualified = qualifier_of(*data) + name;
		if (!data->prefix->prefixes.empty()
			&& data->prefix->prefixes.back().kind == NodePrefix::PrefixKind::Const) {
			return "(const) " + typed(qualified, *data);
		}
		return modifiers_of(*data) + typed(qualified, *data);
	}

	static bool contains(const SourceRange& range, const SourceRange& inner) {
		const auto before = [](const SourcePosition& a, const SourcePosition& b) {
			return a.line < b.line || (a.line == b.line && a.column <= b.column);
		};
		return range.is_valid() && inner.is_valid() && before(range.start, inner.start) && before(inner.end, range.end);
	}

	/// The construct a declaration stands in, as TypeScript's context span: a function or a
	/// block with its body, a variable's declare statement, a parameter with its annotation.
	/// Invalid when no such construct holds the name where the source writes it - a
	/// declaration a macro produced, or one the compiler made.
	static SourceRange context_range_of(NodeAST& target) {
		const auto name = source_range_from_token(target.tok);
		const auto holds_name = [&](const NodeAST& node) {
			return node.tok.file() == target.tok.file() && contains(node.range, name);
		};
		if (auto* header = target.cast<NodeFunctionHeader>()) {
			auto* definition = header->parent ? header->parent->cast<NodeFunctionDefinition>() : nullptr;
			if (definition && holds_name(*definition)) return definition->range;
			return holds_name(*header) ? header->range : SourceRange{};
		}
		if (target.cast<NodeStruct>() || target.cast<NodeNamespace>() || target.cast<NodeFamily>()
			|| target.cast<NodeConst>()) {
			return holds_name(target) ? target.range : SourceRange{};
		}
		for (NodeAST* node = target.parent; node; node = node->parent) {
			if (node->cast<NodeFunctionParam>() || node->cast<NodeSingleDeclaration>()
				|| node->cast<NodeDeclaration>()) {
				return holds_name(*node) ? node->range : SourceRange{};
			}
			// A declaration is a statement of its own; past a block it is none of these.
			if (node->cast<NodeBlock>()) break;
		}
		return {};
	}

	/// Renders the hover of a declaration once, keyed by its name token.
	void record_hover(NodeAST& target) const {
		if (!m_record_hovers || target.tok.file().empty()) return;
		const auto name_range = source_range_from_token(target.tok);
		if (!name_range.is_valid() || m_index.has_hover(target.tok.file(), name_range)) return;
		const auto* data = dynamic_cast<const NodeDataStructure*>(&target);
		m_index.add_hover(target.tok.file(), name_range, hover_of(target),
			data && data->is_function_param() ? target.tok.val : std::string{});
	}

	/// Whether the source writes a name with this token. A node the compiler makes may
	/// borrow the token of what it was made for - FunctionShortCircuit gives the temporaries
	/// of <if not obj.has(x)> the <if> and the <not> - and those are not symbols.
	static bool is_name(const Token& token) {
		return token.type == token::KEYWORD;
	}

	/// Records the name a declaration is written with as a symbol, when the source spells
	/// it there.
	void record_declaration(NodeAST& node, const Token& name) const {
		const auto symbol = symbol_of(node);
		if (!symbol || name.file().empty() || !is_name(as_written(name))) return;
		const auto verified = source_verified_token(as_written(name));
		if (!verified) return;
		m_index.add_symbol(*verified, symbol->kind, symbol->modifiers | SymbolToken::Declaration);
	}

	/// Records a name the language provides - a KSP command or engine variable - where the
	/// source writes it. A builtin has no declaration in a file to link to, so the name is
	/// checked against the builtin's own instead: a call the compiler makes, like the
	/// <get_ui_id> around a control, carries the token of its argument.
	void record_builtin(const Token& reference, const std::string& name, const SymbolKind kind, const unsigned modifiers = 0) const {
		const auto& written = as_written(reference);
		if (written.file().empty() || !is_name(written) || written.val != name) return;
		if (const auto verified = source_verified_token(written)) {
			m_index.add_symbol(*verified, kind, modifiers | SymbolToken::DefaultLibrary);
		}
	}

	void record_declaration(NodeAST& node) const {
		if (m_pass != Pass::References) return;
		record_declaration(node, node.tok);
		record_hover(node);
	}

	[[nodiscard]] static const NodePrefix* prefixes_of(const NodeAST& target) {
		const auto* declaration = dynamic_cast<const NodeDataStructure*>(&target);
		return declaration ? declaration->prefix.get() : nullptr;
	}

	void add_qualifier_links(const Token& reference, const NodeAST& target) const {
		const auto* target_prefixes = prefixes_of(target);
		if (!target_prefixes || target_prefixes->prefixes.empty()) return;

		const auto segments = StringUtils::split(reference.val, '.');
		if (segments.size() < 2) return;

		// A reference may omit leading qualifiers inside its current scope. Align the
		// visible qualifier suffix with the declaration's complete desugared prefix path.
		// The stored tokens identify the exact block even when names and files are equal.
		const size_t visible_prefix_count = segments.size() - 1;
		const size_t comparable_count = std::min(
			visible_prefix_count,
			target_prefixes->prefixes.size());
		const size_t source_start = visible_prefix_count - comparable_count;
		const size_t target_start = target_prefixes->prefixes.size() - comparable_count;

		size_t offset = 0;
		for (size_t i = 0; i < visible_prefix_count; ++i) {
			if (i >= source_start) {
				const auto& declaration = target_prefixes->prefixes[target_start + i - source_start];
				if (segments[i] == declaration.token.val) {
					const auto token = segment_token(reference, offset, segments[i]);
					m_index.add_link(token, declaration.token, symbol_of(declaration.kind).kind);
				}
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
		auto cache = m_source_cache.find(token.file());
		if (cache == m_source_cache.end()) {
			auto loaded = m_sources->load(SourceId(token.file()));
			if (loaded.is_error()) return token;
			cache = m_source_cache.emplace(token.file(), loaded.unwrap().text).first;
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

	/// Links a reference to its declaration.
	///
	/// The name is matched against the declaration, but every range comes from the token as
	/// the source spells it: a name a <#param#> substitution assembled (<#browser#.foo>
	/// becoming <browser.foo>) matches no file's text, while the word it was built from is
	/// exactly what the editor puts the cursor in. The two are the same token everywhere
	/// else, which is why the split only shows up here.
	void add_link(const Token& reference, NodeAST& target) const {
		// Builtins/engine variables and synthesized nodes have no real source file.
		if (reference.file().empty() || target.tok.file().empty()) return;
		const auto& written = as_written(reference);
		if (written.file().empty() || !is_name(written)) return;
		auto verified_reference = source_verified_token(written);
		if (!verified_reference) return;
		const auto def_range = declaration_range(target);
		if (!def_range.is_valid()) return;

		Token direct_reference = *verified_reference;
		// The declaration is either the whole name or its last dotted segment. Which one is
		// decided on the resolved name; the characters to underline come from the written
		// one, whose last segment may read differently (<#browser#> for <browser>).
		if (reference.val != target.tok.val) {
			const auto dot = reference.val.rfind('.');
			if (dot == std::string::npos || reference.val.substr(dot + 1) != target.tok.val) {
				return;
			}
			const auto written_dot = written.val.rfind('.');
			if (written_dot == std::string::npos) return;
			direct_reference = segment_token(
				*verified_reference, written_dot + 1, written.val.substr(written_dot + 1));
			auto verified_direct_reference = source_verified_token(direct_reference);
			if (!verified_direct_reference) return;
			direct_reference = *verified_direct_reference;
		}

		add_qualifier_links(*verified_reference, target);

		// The reference side stays on the token: access-chain members carry per-segment
		// tokens (see the to_method_chain overrides), so the token spans exactly the
		// clickable identifier.
		const auto ref_range = source_range_from_token(direct_reference);
		if (!ref_range.is_valid()) return;
		// the name range carries the exact identifier at the declaration, which rename
		// edits replace; def_range may span the whole header for functions
		const auto context_range = context_range_of(target);
		m_index.add(
			direct_reference.file(), ref_range,
			target.tok.file(), def_range,
			source_range_from_token(target.tok),
			// A macro body spells this reference <#browser#> where the declaration is
			// <browser>; the text is verified, so equality is the whole test.
			direct_reference.val == target.tok.val,
			context_range);
		const auto symbol = symbol_of(target);
		if (symbol) m_index.add_symbol(direct_reference.file(), ref_range, symbol->kind, symbol->modifiers);
		// The definitions pass meets declarations before desugaring and inference.
		if (m_pass == Pass::References) record_hover(target);

		// A substituted name also stands at the call site, where the argument was written -
		// the substitution moved the token there. Only the segment the argument supplied
		// reads back as itself there, and only when the argument is a plain name; the
		// source check rejects everything else, which is what keeps this from underlining
		// the comma after it.
		if (reference.origin && reference.val == target.tok.val) {
			if (const auto at_call_site = source_verified_token(reference)) {
				if (const auto call_site_range = source_range_from_token(*at_call_site);
					call_site_range.is_valid()) {
					m_index.add(
						at_call_site->file(), call_site_range,
						target.tok.file(), def_range,
						source_range_from_token(target.tok), true, context_range);
					if (symbol) {
						m_index.add_symbol(at_call_site->file(), call_site_range, symbol->kind, symbol->modifiers);
					}
				}
			}
		}
	}

	void add_link(const NodeAST& reference, NodeAST& target) const {
		add_link(reference.tok, target);
	}

	void record_type_references(NodeAST& node) const {
		if (m_pass != Pass::References || !m_program) return;
		for (const auto& reference : node.type_references) {
			if (!reference.type || reference.type->get_type_kind() != TypeKind::Object) continue;
			auto* definition = m_program->find_struct(reference.type->ksp_encoded_string());
			if (!definition) continue;
			add_link(reference.token, *definition);
		}
	}

	void record_variable(const NodeReference& reference) const {
		if (const auto declaration = reference.get_declaration()) {
			if (declaration->kind == NodeDataStructure::Kind::Builtin) {
				const unsigned readonly = declaration->data_type == DataType::Const ? SymbolToken::Readonly : 0u;
				record_builtin(reference.tok, declaration->name, SymbolKind::Variable, readonly);
				return;
			}
			// Struct desugaring injects a synthetic `self` declaration whose token is the
			// struct name token. Indexing self -> that declaration would make the range-based
			// symbol identity conflate every `self` use with the struct itself.
			if (declaration->name == NodeStruct::SELF && declaration->tok.val != NodeStruct::SELF) return;
			add_link(reference, *declaration);
		}
	}

	template<typename Node>
	NodeAST* record_definition(Node& node, const Token& declaration) {
		if (m_pass == Pass::Definitions) {
			if (node.tok.file() == declaration.file()) {
				m_index.add_context(declaration.file(), source_range_from_token(declaration), node.range);
			}
			add_link(declaration, node);
			record_declaration(node, declaration);
			// Desugaring dissolves the block, so this pass is the only one that sees it.
			record_hover(node);
		}
		ASTVisitor::visit(node);
		return &node;
	}

public:
	explicit ReferenceIndexBuilder(
		ReferenceIndex& index, const Pass pass, SourceProvider* sources = nullptr, const bool record_hovers = true)
		: m_index(index), m_pass(pass), m_sources(sources), m_record_hovers(record_hovers) {}

	NodeAST* visit(NodeVariable& node) override {
		record_declaration(node);
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodePointer& node) override {
		record_declaration(node);
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeArray& node) override {
		record_declaration(node);
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeNDArray& node) override {
		record_declaration(node);
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeList& node) override {
		record_declaration(node);
		record_type_references(node);
		return ASTVisitor::visit(node);
	}
	NodeAST* visit(NodeFunctionHeader& node) override {
		if (m_pass == Pass::Definitions) {
			add_link(node.tok, node);
		}
		record_declaration(node);
		record_type_references(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeStruct& node) override {
		record_declaration(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if (m_pass == Pass::References) record_variable(node);
		// <List<int>.MAX>: the qualifier has no declaration to link to, only the type it names.
		record_type_references(node);
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

	/// Desugaring rebuilds a declare statement into single declarations that carry only its
	/// first token, so the statement as written is recorded now, for every name it declares.
	NodeAST* visit(NodeDeclaration& node) override {
		if (m_pass == Pass::Definitions) {
			for (const auto& variable : node.variable) {
				if (!variable) continue;
				const auto* control = variable->cast<NodeUIControl>();
				const auto& name = control && control->control_var ? control->control_var->tok : variable->tok;
				const auto name_range = source_range_from_token(name);
				if (name.file() == node.tok.file() && contains(node.range, name_range)) {
					m_index.add_context(name.file(), name_range, node.range);
				}
			}
		}
		return ASTVisitor::visit(node);
	}

	/// A control array is split into single controls and an array of their ids before the
	/// reference pass, which then sees nothing but an <int[]>. The control as declared is
	/// only visible here, so its hover is taken now; the later pass does not replace it.
	NodeAST* visit(NodeUIControl& node) override {
		if (m_pass == Pass::Definitions && m_record_hovers && node.control_var) {
			const auto& name = node.control_var->tok;
			std::string dimensions;
			if (node.control_var->cast<NodeArray>()) {
				dimensions = "[]";
			} else if (const auto* nd_array = node.control_var->cast<NodeNDArray>()) {
				const auto rank = nd_array->sizes ? nd_array->sizes->params.size() : 2;
				dimensions = "[" + std::string(rank > 0 ? rank - 1 : 0, ',') + "]";
			}
			const auto range = source_range_from_token(name);
			if (!name.file().empty() && range.is_valid() && !m_index.has_hover(name.file(), range)) {
				m_index.add_hover(name.file(), range,
					modifiers_of(*node.control_var) + node.ui_control_type + " " + name.val + dimensions);
			}
		}
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeNamespace& node) override {
		return record_definition(node, node.prefix);
	}

	NodeAST* visit(NodeFamily& node) override {
		return record_definition(node, node.prefix);
	}

	NodeAST* visit(NodeConst& node) override {
		return record_definition(node, node.const_prefix);
	}

	/// <knob -> value>: a builtin accessor of the control parameter it names.
	void record_control_access(const Token& arrow, const Token& parameter, const std::string& name) const {
		if (m_pass != Pass::References) return;
		if (const auto verified = source_verified_token(arrow); verified && !verified->file().empty()) {
			m_index.add_symbol(*verified, SymbolKind::Operator, SymbolToken::DefaultLibrary);
		}
		record_builtin(parameter, name, SymbolKind::Variable, SymbolToken::Readonly);
	}

	NodeAST* visit(NodeGetControl& node) override {
		record_control_access(node.arrow_token, node.control_param_token, node.control_param);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeSetControl& node) override {
		record_control_access(node.arrow_token, node.control_param_token, node.control_param);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		// <Property> marks the SublimeKSP shorthands like <set_bounds> the compiler expands.
		const bool provided = node.kind == NodeFunctionCall::Kind::Builtin || node.kind == NodeFunctionCall::Kind::Property;
		if (m_pass == Pass::References && node.function && provided) {
			record_builtin(node.function->tok, node.function->name, SymbolKind::Function);
		}
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
