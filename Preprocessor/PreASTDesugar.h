//
// Created by Mathias Vatter on 10.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTDesugar : public PreASTVisitor {
public:
    std::vector<Token> m_tokens;

    void visit(PreNodeNumber& node) override {
        m_tokens.push_back(std::move(node.number));
    }
    void visit(PreNodeKeyword& node) override {
        m_tokens.push_back(std::move(node.keyword));
    }
    void visit(PreNodeOther& node) override {
        m_tokens.push_back(std::move(node.other));
    }
    void visit(PreNodeStatement& node) override {
        node.statement->accept(*this);
    }
//    void visit(PreNodeChunk& node) override {
//    }
    void visit(PreNodeDefineHeader& node) override {

    }
    void visit(PreNodeList& node) override {

    }
    void visit(PreNodeDefineStatement& node) override {

    }
    void visit(PreNodeDefineCall& node) override {

    }
//    void visit(PreNodeProgram& node) override {
//
//    }
};

