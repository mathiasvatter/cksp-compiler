//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDesugar : public PreASTVisitor {
public:

    void visit(PreNodeNumber& node) override;
    void visit(PreNodeInt& node) override;
    void visit(PreNodeKeyword& node) override;
    void visit(PreNodeOther& node) override;
    void visit(PreNodeStatement& node) override;
    void visit(PreNodeChunk& node) override;
    void visit(PreNodeDefineHeader& node) override;
    void visit(PreNodeList& node) override;
    void visit(PreNodeDefineStatement& node) override;
    void visit(PreNodeDefineCall& node) override;
    void visit(PreNodeMacroCall& node) override;
    void visit(PreNodeIterateMacro& node) override;
    void visit(PreNodeLiterateMacro& node) override;

    void visit(PreNodeProgram& node) override;

private:
    PreNodeProgram* m_main_ptr;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> m_define_definitions;
    std::vector<std::unique_ptr<PreNodeMacroDefinition>> m_macro_definitions;

    std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
    template<typename T> static std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>> get_substitution_vector(T* definition, T* call);
    std::unique_ptr<PreNodeDefineStatement> get_define_definition(PreNodeDefineHeader* define_header);
    std::unique_ptr<PreNodeMacroDefinition> get_macro_definition(PreNodeMacroHeader* macro_header);
    std::string get_text_replacement(const std::string& name);

    std::stack<std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>>> m_substitution_stack;
    std::vector<std::string> m_define_call_stack;
    std::vector<std::string> m_macro_call_stack;


};

class PreASTCombine : public PreASTVisitor {
public:
    std::vector<Token> m_tokens;

    void visit(PreNodeNumber& node) override;
    void visit(PreNodeKeyword& node) override;
    void visit(PreNodeOther& node) override;
    void visit(PreNodeProgram& node) override;
};

