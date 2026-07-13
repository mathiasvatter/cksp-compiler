//
// Created by Mathias Vatter on 23.01.24.
//

#pragma once

#include "PreASTVisitor.h"

class ReferenceIndex;

class PreASTDefines final : public PreASTVisitor {
public:
	/// The optional reference index collects define-usage -> define-definition links for
	/// go-to-definition while substituting (language server only).
	explicit PreASTDefines(ReferenceIndex* reference_index = nullptr)
		: m_reference_index(reference_index) {}

	// transform to define calls if define definition exists, otherwise return node
	PreNodeAST *visit(PreNodeFunctionCall &node) override;
	PreNodeAST *visit(PreNodeNumber &node) override;
	PreNodeAST *visit(PreNodeInt &node) override;
	// transform to define calls if define definition exists, otherwise return node
	PreNodeAST *visit(PreNodeKeyword &node) override;
	PreNodeAST *visit(PreNodeOther &node) override;
	PreNodeAST *visit(PreNodeChunk &node) override;
	PreNodeAST *visit(PreNodeDefineStatement &node) override;
	PreNodeAST *visit(PreNodeDefineCall &node) override;
	PreNodeAST *visit(PreNodeProgram &node) override;

private:
	std::string m_debug_token;
	ReferenceIndex* m_reference_index = nullptr;
	// header argument tokens of the define currently being substituted, keyed by argument
	// name; kept parallel to m_substitution_stack so argument usages inside a define body
	// can be linked to the header argument for go-to-definition
	std::stack<std::unordered_map<std::string, Token>> m_param_token_stack;

	PreNodeAST *do_substitution(PreNodeLiteral &node);
	std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
	static std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> get_substitution_map(PreNodeDefineHeader& definition, const PreNodeDefineHeader& call);

	std::unordered_set<std::string> m_defines_used;
	void check_recursion(const Token &tok) const;

	std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> m_builtin_defines;
	static std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> get_builtin_defines();
	std::unique_ptr<PreNodeAST> get_builtin_define(const std::string& keyword);

};

