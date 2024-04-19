//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDesugar : public PreASTVisitor {
public:

    void visit(PreNodeProgram& node) override;
    void visit(PreNodeNumber& node) override;
    void visit(PreNodeInt& node) override;
    void visit(PreNodeKeyword& node) override;
    void visit(PreNodeOther& node) override;
    void visit(PreNodeStatement& node) override;
    void visit(PreNodeChunk& node) override;
//    void visit(PreNodeDefineHeader& node) override;
    void visit(PreNodeList& node) override;
//    void visit(PreNodeDefineStatement& node) override;
//    void visit(PreNodeDefineCall& node) override;
    void visit(PreNodeMacroCall& node) override;
	void visit(PreNodeMacroHeader& node) override;
    void visit(PreNodeIterateMacro& node) override;
    void visit(PreNodeLiterateMacro& node) override;
	void visit(PreNodeIncrementer& node) override;


private:
	std::string m_debug_token;

    PreNodeProgram* m_main_ptr = nullptr;
//    std::vector<std::unique_ptr<PreNodeMacroDefinition>> m_macro_definitions;
	std::unordered_map<StringIntKey, PreNodeMacroDefinition*, StringIntKeyHash> m_macro_lookup;
    std::unordered_map<std::string, PreNodeMacroDefinition*> m_macro_string_lookup;

    std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
    static std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> get_substitution_vector(PreNodeMacroHeader* definition, PreNodeMacroHeader* call);
    std::unique_ptr<PreNodeMacroDefinition> get_macro_definition(PreNodeMacroHeader* macro_header);
    std::string get_text_replacement(const Token& name);

    std::stack<std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>>> m_substitution_stack;
    std::vector<std::string> m_define_call_stack;
    std::vector<std::string> m_macro_call_stack;
};



