//
// Created by Mathias Vatter on 02.12.23.
//

#pragma once

#include "ASTVisitor.h"

class ASTTypeChecking : public ASTVisitor {
public:
    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeUIControl& node) override;
//    void visit(NodeParamList& node) override;
//    void visit(NodeSingleAssignStatement& node) override;
    void visit(NodeVariable& node) override;
    void visit(NodeArray& node) override;
//    void visit(NodeInt& node) override;
//    void visit(NodeString& node) override;
//    void visit(NodeReal& node) override;
//    void visit(NodeBinaryExpr& node) override;
//    void visit(NodeUnaryExpr& node) override;
//    void visit(NodeFunctionCall& node) override;
//    void visit(NodeFunctionHeader& node) override;
//	void visit(NodeStatementList& node) override;
//	void visit(NodeStatement& node) override;


private:
    int m_max_returns_in_current_callback = 0;
    int m_current_return_idx = 0;
    static int extract_last_number(const std::string& str, NodeAST* var);

};


