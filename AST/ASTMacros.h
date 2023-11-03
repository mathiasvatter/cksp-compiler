//
// Created by Mathias Vatter on 03.11.23.
//

#pragma once


#include "ASTVisitor.h"

class ASTMacros : public ASTVisitor {
    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeFunctionDefinition& node) override;
    void visit(NodeMacroCall& node) override;

private:
    NodeProgram* m_main_ptr;
    std::vector<std::unique_ptr<NodeCallback>> m_callbacks;
    std::vector<std::unique_ptr<NodeFunctionDefinition>> m_function_definitions;
    std::vector<std::unique_ptr<NodeMacroDefinition>> m_macro_definitions;

    std::stack<std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>>> m_substitution_stack;
    std::vector<std::string> m_macro_call_stack;


    std::unique_ptr<NodeMacroDefinition> get_macro_definition(NodeMacroHeader* macro_header);
    bool is_program_level_macro(NodeMacroCall* macro_call);
    std::vector<std::pair<std::string, std::unique_ptr<NodeAST>>> get_substitution_vector(NodeMacroHeader* definition, NodeMacroHeader* call);
    /// returns substitute for current node.name, or nullptr if there is no substitute
    std::unique_ptr<NodeAST> get_substitute(const std::string& name);
};

