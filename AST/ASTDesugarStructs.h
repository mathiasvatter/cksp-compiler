//
// Created by Mathias Vatter on 21.01.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTDesugarStructs: public ASTVisitor {
    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;

    /// turn into single declare statements
    void visit(NodeDeclareStatement& node) override;
    /// turn into single assign statements
    void visit(NodeAssignStatement& node) override;

    /// replace const block with single declare statements
    void visit(NodeConstStatement& node) override;
    /// replace family block with single declare statements
    void visit(NodeFamilyStatement& node) override;
    /// alter for loops to while loops
    void visit(NodeForStatement& node) override;
	void visit(NodeRangedForStatement& node) override;

    void visit(NodeListStatement& node) override;
    void visit(NodeStatementList& node) override;
    /// Ingest type definition character and add family/const prefixes
    void visit(NodeArray& node) override;
    /// Ingest type definition character and add family/const prefixes
    void visit(NodeVariable& node) override;

private:
    NodeProgram* m_program = nullptr;
    std::stack<std::string> m_family_prefixes;
    std::stack<std::string> m_const_prefixes;

};

