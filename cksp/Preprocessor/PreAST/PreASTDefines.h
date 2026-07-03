//
// Created by Mathias Vatter on 23.01.24.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDefines final : public PreASTVisitor {
public:

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

	PreNodeAST *do_substitution(PreNodeLiteral &node);
	std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
	static std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> get_substitution_map(PreNodeDefineHeader& definition, const PreNodeDefineHeader& call);

	std::unordered_set<std::string> m_defines_used;
	void check_recursion(const Token &tok) const;

	std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> m_builtin_defines;
	static std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> get_builtin_defines();
	std::unique_ptr<PreNodeAST> get_builtin_define(const std::string& keyword);

};

