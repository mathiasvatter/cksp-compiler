//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

//#include "ASTVisitor.h"
//#include "AST.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"
#include "../Lowering/ASTHandler.h"

class ASTLowering: public ASTVisitor {
public:
    explicit ASTLowering(DefinitionProvider* definition_provider);

    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeBody& node) override;
//    void visit(NodeStatement& node) override;
    void visit(NodeUIControl& node) override;
//    void visit(NodeArray& node) override;
	void visit(NodeNDArray& node) override;
//    void visit(NodeVariable& node) override;

private:
    DefinitionProvider* m_def_provider;
//    NDArrayHandler m_array_handler;


    /// will be added to the next statement
//    std::unique_ptr<NodeBody> m_body_to_add = nullptr;
};


