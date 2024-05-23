//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTVariableChecking : public ASTVisitor {
public:
	explicit ASTVariableChecking(DefinitionProvider* definition_provider);

	void visit(NodeProgram& node) override;
	/// check if on init callback currently
	void visit(NodeCallback& node) override;
	/// Check for correct variable types and parameter number
	void visit(NodeUIControl& node) override;
	/// Scoping
	void visit(NodeBody& node) override;
    /// decide if declaration is local or global
    void visit(NodeSingleDeclareStatement& node) override;
	/// Check if correctly declared and save declaration
	void visit(NodeArray& node) override;
    /// get declaration
    void visit(NodeArrayRef& node) override;
	/// Check if correctly declared. Replace with Array when no brackets are used
	void visit(NodeVariable& node) override;
    /// get declaration
    void visit(NodeVariableRef& node) override;
	/// handle get_ui_id specific checks. Replace variable parameter when in get_ui_id and not ui_control
	void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionDefinition& node) override;

private:
	NodeProgram* m_program = nullptr;
	NodeCallback* m_current_callback = nullptr;
    NodeBody* m_current_body = nullptr;
	bool m_is_init_callback = false;
	DefinitionProvider* m_def_provider = nullptr;

};