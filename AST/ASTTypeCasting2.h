//
// Created by Mathias Vatter on 30.03.24.
//
#pragma once

#include "ASTVisitor.h"

/// Better typecasting by casting types normally, searching for builtin functions
/// and variables -> getting variable declarations
class ASTTypeCasting2 : public ASTVisitor {
public:
    explicit ASTTypeCasting2();

    void visit(NodeProgram& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeSingleAssignStatement& node) override;
//    void visit(NodeUIControl& node) override;
//    void visit(NodeParamList& node) override;
//    void visit(NodeVariable& node) override;
//    void visit(NodeArray& node) override;
//    void visit(NodeInt& node) override;
//    void visit(NodeString& node) override;
//    void visit(NodeReal& node) override;
//    void visit(NodeBinaryExpr& node) override;
//    void visit(NodeUnaryExpr& node) override;
//    void visit(NodeFunctionCall& node) override;
//    void visit(NodeFunctionHeader& node) override;
//    void visit(NodeStatementList& node) override;
//	void visit(NodeStatement& node) override;



    /// handle two nodes that should have the same type
    ASTType match_types(NodeAST* probably_typed, NodeAST* not_typed);
    static bool type_mismatch(NodeAST* node1, NodeAST* node2);
private:


};


