//
// Created by Mathias Vatter on 23.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class TypeCasting : public ASTVisitor {
public:
	explicit TypeCasting(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {};

	void visit(NodeProgram& node) override;

	/// Type casting with Base Types
	void visit(NodeInt& node) override;
	void visit(NodeString& node) override;
	void visit(NodeReal& node) override;

	void visit(NodeSingleDeclareStatement& node) override;
//	void visit(NodeSingleAssignStatement& node) override;
//	void visit(NodeUIControl& node) override;
//	void visit(NodeParamList& node) override;
//	void visit(NodeVariableRef& node) override;
//	void visit(NodeVariable& node) override;
//	void visit(NodeArray& node) override;
//	void visit(NodeArrayRef& node) override;
//	void visit(NodeBinaryExpr& node) override;
//	void visit(NodeUnaryExpr& node) override;
//	void visit(NodeFunctionCall& node) override;
//	void visit(NodeFunctionHeader& node) override;
//	void visit(NodeBody& node) override;

private:
	DefinitionProvider* m_def_provider;
	CompileError error = CompileError(ErrorType::TypeError,"", "", m_program->tok);
};