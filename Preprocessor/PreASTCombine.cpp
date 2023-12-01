//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreASTCombine.h"


void PreASTCombine::visit(PreNodeNumber &node) {
    m_tokens.push_back(std::move(node.number));
}

void PreASTCombine::visit(PreNodeInt &node) {
    m_tokens.push_back(std::move(node.number));
}

void PreASTCombine::visit(PreNodeKeyword &node) {
    m_tokens.push_back(std::move(node.keyword));
}

void PreASTCombine::visit(PreNodeOther &node) {
    m_tokens.push_back(std::move(node.other));
}

void PreASTCombine::visit(PreNodeProgram& node) {
    for(auto & n : node.program) {
        n->accept(*this);
    }
};

void PreASTCombine::visit(PreNodeUnaryExpr& node) {
    m_tokens.push_back(std::move(node.op));
    node.operand->accept(*this);
};

void PreASTCombine::visit(PreNodeBinaryExpr& node) {
    node.left->accept(*this);
    m_tokens.push_back(std::move(node.op));
    node.right->accept(*this);
};

void PreASTCombine::visit(PreNodeIncrementer& node) {
    for(auto & b : node.body) {
        b->accept(*this);
    }
}
