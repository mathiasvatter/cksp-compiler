//
// Created by Mathias Vatter on 18.11.23.
//

#pragma once

#include "PreASTVisitor.h"

class PreASTCombine final : public PreASTVisitor {
public:
    std::vector<Token> m_tokens;
    PreNodeAST *visit(PreNodeChunk &node) override;
    PreNodeAST *visit(PreNodeNumber &node) override;
    PreNodeAST *visit(PreNodeInt &node) override;
    PreNodeAST *visit(PreNodeKeyword &node) override;
    PreNodeAST *visit(PreNodeOther &node) override;
    PreNodeAST *visit(PreNodeProgram &node) override;
    PreNodeAST *visit(PreNodeUnaryExpr &node) override;
    PreNodeAST *visit(PreNodeBinaryExpr &node) override;
    PreNodeAST *visit(PreNodeIncrementer &node) override;
    PreNodeAST *visit(PreNodeList &node) override;
    PreNodeAST *visit(PreNodeMacroHeader &node) override;

};

