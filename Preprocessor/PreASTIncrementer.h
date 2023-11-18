//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTIncrementer : public PreASTVisitor {
public:
    void visit(PreNodeNumber& node) override;
    void visit(PreNodeInt& node) override;
    void visit(PreNodeKeyword& node) override;
    void visit(PreNodeIncrementer& node) override;
    void visit(PreNodeProgram& node) override;

private:
    std::stack<std::vector<std::pair<std::string, std::unique_ptr<PreNodeChunk>>>> m_incrementer_stack;
    std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);

};
