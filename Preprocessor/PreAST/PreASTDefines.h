//
// Created by Mathias Vatter on 23.01.24.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDefines final : public PreASTVisitor {
public:
	explicit PreASTDefines(PreNodeProgram* program) : PreASTVisitor(program) {}
	void visit(PreNodePragma& node) override;
	void visit(PreNodeNumber& node) override;
	void visit(PreNodeInt& node) override;
	void visit(PreNodeKeyword& node) override;
	void visit(PreNodeOther& node) override;
	void visit(PreNodeChunk& node) override;
	void visit(PreNodeDefineHeader& node) override;
	void visit(PreNodeList& node) override;
	void visit(PreNodeDefineStatement& node) override;
	void visit(PreNodeDefineCall& node) override;

	void visit(PreNodeProgram& node) override;

private:
	std::string m_debug_token;

	std::unordered_map<std::string, PreNodeDefineStatement*> m_define_lookup;

	void do_substitution(PreNodeLiteral& node);
	std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
	std::unique_ptr<PreNodeDefineStatement> get_define_definition(const PreNodeDefineHeader& define_header);
	static std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> get_substitution_map(PreNodeDefineHeader& definition, const PreNodeDefineHeader& call);

	/// returns substitute for current node.name, or nullptr if there is no substitute
	std::stack<std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>>> m_substitution_stack;

	std::unordered_set<std::string> m_defines_used;
	void check_recursion(const Token &tok) const;

	std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> m_builtin_defines;
	static std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> get_builtin_defines();
	std::unique_ptr<PreNodeAST> get_builtin_define(const std::string& keyword);

};

