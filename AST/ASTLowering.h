//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

//#include "ASTVisitor.h"
//#include "AST.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"
#include "ASTHandler.h"

class ASTLowering: public ASTVisitor {
public:
    explicit ASTLowering(DefinitionProvider* definition_provider);

//    void visit(NodeUIControl& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
//    void visit(NodeSingleAssignStatement& node) override;
//    void visit(NodeParamList& node) override;

//    void visit(NodeGetControlStatement& node) override;

//    void visit(NodeForStatement& node) override;
//    void visit(NodeWhileStatement& node) override;
//    void visit(NodeIfStatement& node) override;
//    void visit(NodeSelectStatement& node) override;

//    void visit(NodeListStatement& node) override;
    void visit(NodeBody& node) override;
//    void visit(NodeStatement& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

private:
    DefinitionProvider* m_def_provider;
//    ArrayHandler m_array_handler;


    /// will be added to the next statement
//    std::unique_ptr<NodeBody> m_body_to_add = nullptr;
};


