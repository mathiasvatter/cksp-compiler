//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class ReferenceIndex;

class PreASTMacros final : public PreASTVisitor {
public:
	/// The optional reference index collects macro-usage -> macro-definition links for
	/// go-to-definition while expanding (language server only).
	/// <expand_post_macros>: expand <iterate_post_macro> and <literate_post_macro> instead of
	/// leaving them in place. The pass that expands ordinary macros runs with this off, so the
	/// post variants see their parameters substituted before they are expanded by a later pass.
	explicit PreASTMacros(ReferenceIndex* reference_index = nullptr, const bool expand_post_macros = false)
		: m_expand_post_macros(expand_post_macros), m_reference_index(reference_index) {}

	/// number of post macros this pass left in place for a later one
	[[nodiscard]] int get_deferred_post_macros() const { return m_deferred_post_macros; }

	// transform to macro calls if macro definition exists, otherwise return node
	PreNodeAST *visit(PreNodeFunctionCall &node) override;

	PreNodeAST *visit(PreNodeProgram &node) override;
    PreNodeAST *visit(PreNodeNumber &node) override;
    PreNodeAST *visit(PreNodeInt &node) override;
    PreNodeAST *visit(PreNodeKeyword &node) override;
    PreNodeAST *visit(PreNodeOther &node) override;
    PreNodeAST *visit(PreNodeStatement &node) override;
    PreNodeAST *visit(PreNodeChunk &node) override;
    PreNodeAST *visit(PreNodeList &node) override;
	PreNodeAST *visit(PreNodeMacroCall &node) override;
	PreNodeAST *visit(PreNodeMacroHeader &node) override;
	PreNodeAST *visit(PreNodeIterateMacro &node) override;
	PreNodeAST *visit(PreNodeLiterateMacro &node) override;
	PreNodeAST *visit(PreNodeIncrementer &node) override;


private:
	std::string m_debug_token;
	bool m_expand_post_macros = false;
	int m_deferred_post_macros = 0;
	/// set while the children of a deferred post macro are visited: parameters are substituted,
	/// but nothing is expanded - the callee still has to stand in for <#n#> in the later pass
	bool m_defer_expansion = false;
	ReferenceIndex* m_reference_index = nullptr;
	// header parameter tokens of the macro currently being expanded, keyed by parameter name;
	// kept parallel to m_substitution_stack so parameter usages inside a macro body can be
	// linked to the header parameter for go-to-definition
	std::stack<std::unordered_map<std::string, Token>> m_param_token_stack;

	// std::unordered_map<StringIntKey, PreNodeMacroDefinition*, StringIntKeyHash> m_macro_lookup;

	// macro lookup without args because iterate and literate macro calls do not have args in their constructs
    std::unordered_map<std::string, PreNodeMacroDefinition*> m_macro_string_lookup;
	// only uses macro name without args for lookup
	PreNodeMacroDefinition* get_macro_string_definition(const PreNodeMacroHeader& macro_header);

	/// Leaves a post macro in place for the post pass, after substituting the macro parameters
	/// its bounds and its callee are built from. Returns whether the node was deferred.
	template<typename Substitute>
	bool defer_post_macro(const bool is_post, Substitute&& substitute) {
		if (!is_post or m_expand_post_macros) return false;
		m_deferred_post_macros++;
		const bool was_deferring = m_defer_expansion;
		m_defer_expansion = true;
		substitute();
		m_defer_expansion = was_deferring;
		return true;
	}

	PreNodeAST *do_substitution(PreNodeLiteral &node);
	void link_parameter_groups(const Token& word) const;
    std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
    std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> get_substitution_map(PreNodeMacroHeader& definition, const PreNodeMacroHeader& call) const;
    // PreNodeMacroDefinition* get_macro_definition(const PreNodeMacroHeader& macro_header);



    void check_recursion(const Token &tok) const;
    std::unordered_set<std::string> m_macros_used;
};


