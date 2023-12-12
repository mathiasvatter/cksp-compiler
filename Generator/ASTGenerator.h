//
// Created by Mathias Vatter on 21.11.23.
//

#pragma once

#include <sstream>
#include "../AST/ASTVisitor.h"

class ASTGenerator : public ASTVisitor {
public:
    void visit(NodeProgram& node) override;

    void visit(NodeInt& node) override;

    void visit(NodeReal& node) override;

    void visit(NodeString& node) override;

    void visit(NodeVariable& node) override;

    void visit(NodeArray& node) override;

    void visit(NodeUIControl& node) override;

    void visit(NodeSingleDeclareStatement& node) override;

    void visit(NodeParamList& node) override;

    void visit(NodeBinaryExpr& node) override;

    void visit(NodeUnaryExpr& node) override;

    void visit(NodeSingleAssignStatement& node) override;

    void visit(NodeStatement& node) override;

    void visit(NodeIfStatement& node) override;

    void visit(NodeWhileStatement& node) override;

    void visit(NodeSelectStatement& node) override;

    void visit(NodeCallback& node) override;

    void visit(NodeFunctionHeader& node) override;

    void visit(NodeFunctionCall& node) override;

    void visit(NodeFunctionDefinition& node) override;

    void visit(NodeGetControlStatement& node) override;

    void visit(NodeSetControlStatement& node) override;

    std::ostringstream os;

	void generate(const std::string& path) const;
	void print() const;
};


