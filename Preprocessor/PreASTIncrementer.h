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
    std::vector<std::tuple<std::string, int32_t, std::unique_ptr<PreNodeChunk>>> m_incrementer_stack;
    int find_substitute(const std::string& name);
    std::vector<std::pair<std::unique_ptr<PreNodeAST>, size_t>> m_last_incrementer_var;
    std::unique_ptr<PreNodeAST> get_substitute(const std::string& name);
    void update_last_incrementer_var(PreNodeAST& node, const std::string& node_val, size_t node_line);
};
