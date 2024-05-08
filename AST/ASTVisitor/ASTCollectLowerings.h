//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"
#include "../../Lowering/ASTLowering.h"

class ASTCollectLowerings: public ASTVisitor {
public:
    explicit ASTCollectLowerings(DefinitionProvider* definition_provider);

	/// lower ndarray when declaration or ui_control array or determine size of array in declaration
    void visit(NodeSingleDeclareStatement& node) override;
	/// lower ndArray when they are a reference
	void visit(NodeNDArray& node) override;

    void visit(NodeConstStatement& node) override;
    void visit(NodeFamilyStatement& node) override;

	void visit(NodeListStructRef& node) override;
	void visit(NodeListStruct& node) override;

private:
    DefinitionProvider* m_def_provider;
};


