//
// Created by Mathias Vatter on 31.10.23.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTTypeCasting : public ASTVisitor {
public:
    explicit ASTTypeCasting(DefinitionProvider* definition_provider);

    void visit(NodeProgram& node) override;
    void visit(NodeSingleDeclaration& node) override;
    void visit(NodeUIControl& node) override;
    void visit(NodeParamList& node) override;
    void visit(NodeSingleAssignment& node) override;
    void visit(NodeVariableRef& node) override;
    void visit(NodeVariable& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeArrayRef& node) override;
    void visit(NodeInt& node) override;
    void visit(NodeString& node) override;
    void visit(NodeReal& node) override;
    void visit(NodeBinaryExpr& node) override;
    void visit(NodeUnaryExpr& node) override;
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionHeader& node) override;
	void visit(NodeBody& node) override;
//	void visit(NodeStatement& node) override;

private:
	DefinitionProvider* m_def_provider;
    std::unordered_map<int, ASTType> m_return_variables;

};

