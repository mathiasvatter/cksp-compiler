//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTCombine : public PreASTVisitor {
public:
    std::vector<Token> m_tokens;

	void visit(PreNodeChunk& node) override;
    void visit(PreNodeNumber& node) override;
    void visit(PreNodeInt& node) override;
    void visit(PreNodeKeyword& node) override;
    void visit(PreNodeOther& node) override;
    void visit(PreNodeProgram& node) override;
    void visit(PreNodeUnaryExpr& node) override;
    void visit(PreNodeBinaryExpr& node) override;
    void visit(PreNodeIncrementer& node) override;
	void visit(PreNodeList& node) override;
	void visit(PreNodeMacroHeader& node) override;

};

