//
// Created by Mathias Vatter on 23.01.24.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDefines : public PreASTVisitor {
public:

	void visit(PreNodeNumber& node) override;
	void visit(PreNodeInt& node) override;
	void visit(PreNodeKeyword& node) override;
	void visit(PreNodeOther& node) override;
//	void visit(PreNodeStatement& node) override;
	void visit(PreNodeChunk& node) override;
	void visit(PreNodeDefineHeader& node) override;
	void visit(PreNodeList& node) override;
	void visit(PreNodeDefineStatement& node) override;
	void visit(PreNodeDefineCall& node) override;
//	void visit(PreNodeMacroCall& node) override;
//	void visit(PreNodeIterateMacro& node) override;
//	void visit(PreNodeLiterateMacro& node) override;
//	void visit(PreNodeIncrementer& node) override;

	void visit(PreNodeProgram& node) override;

private:
	std::string m_debug_token;

	PreNodeProgram* m_main_ptr = nullptr;
	std::vector<std::unique_ptr<PreNodeDefineStatement>> m_define_definitions;
	std::unordered_map<std::string, PreNodeDefineStatement*> m_define_lookup;

	std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
	std::unique_ptr<PreNodeDefineStatement> get_define_definition(PreNodeDefineHeader* define_header);
	static std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>> get_substitution_vector(PreNodeDefineHeader* definition, PreNodeDefineHeader* call);

	/// returns substitute for current node.name, or nullptr if there is no substitute
	std::stack<std::unordered_map<std::string, std::unique_ptr<PreNodeChunk>>> m_substitution_stack;

	std::vector<std::string> m_define_call_stack;

	std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> m_builtin_defines;
	static std::unordered_map<std::string, std::unique_ptr<PreNodeAST>> get_builtin_defines();
	std::unique_ptr<PreNodeAST> get_builtin_define(const std::string& keyword);

};

