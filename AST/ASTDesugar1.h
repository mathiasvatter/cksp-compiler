//
// Created by Mathias Vatter on 21.01.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTDesugar1: public ASTVisitor {
    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;

//    void visit(NodeSingleDeclareStatement& node) override;
//    void visit(NodeSingleAssignStatement& node) override;
//    void visit(NodeParamList& node) override;

//    void visit(NodeGetControlStatement& node) override;
    void visit(NodeDeclareStatement& node) override;
    void visit(NodeAssignStatement& node) override;

    /// replace const block with single declare statements
    void visit(NodeConstStatement& node) override;
    /// replace family block with single declare statements
    void visit(NodeFamilyStatement& node) override;
    /// alter for loops to while loops

//    void visit(NodeForStatement& node) override;
//    void visit(NodeWhileStatement& node) override;
//    void visit(NodeIfStatement& node) override;

    void visit(NodeListStatement& node) override;
    void visit(NodeStatementList& node) override;
//    void visit(NodeStatement& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

private:
    NodeProgram* m_program = nullptr;
    std::stack<std::string> m_family_prefixes;
    std::stack<std::string> m_const_prefixes;

};

