//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "AST.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"

/// complete ASTNodes like arrays, ui controls, etc., fill in declaration information by
/// tracking data structure definitions with DefinitionProvider
class ASTBuildDataStructures: public ASTVisitor {
public:
	explicit ASTBuildDataStructures(DefinitionProvider* definition_provider);
	void visit(NodeBody& node) override;
	void visit(NodeUIControl& node) override;
	void visit(NodeArray& node) override;
	void visit(NodeNDArray& node) override;
	void visit(NodeVariable& node) override;

private:
	DefinitionProvider* m_def_provider;
};

