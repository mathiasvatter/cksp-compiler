//
// Created by Mathias Vatter on 31.10.23.
//

#pragma once

#include "ASTVisitor.h"

class ASTTypeCasting : public ASTVisitor {
public:
	void visit(NodeArray& node) override;
	void visit(NodeVariable& node) override;
    void visit(NodeBinaryExpr& node) override;
    void visit(NodeUnaryExpr& node) override;

    void visit(NodeParamList& node) override;
	void visit(NodeStatementList& node) override;
//	void visit(NodeStatement& node) override;

};

