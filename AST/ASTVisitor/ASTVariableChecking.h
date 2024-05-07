//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTVariableChecking : public ASTVisitor {
public:
	ASTVariableChecking(DefinitionProvider* definition_provider, std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> m_function_inlines);

	void visit(NodeProgram& node) override;
	/// check if on init callback currently
	void visit(NodeCallback& node) override;
	/// Function inlining checking
	void visit(NodeStatement& node) override;
	/// Check for correct variable types and parameter number
	void visit(NodeUIControl& node) override;
	/// Scoping
	void visit(NodeBody& node) override;
	/// Check if correctly declared
	void visit(NodeArray& node) override;
	/// Check if correctly declared. Replace with Array when no brackets are used
	void visit(NodeVariable& node) override;
	/// handle get_ui_id specific checks. Replace variable parameter when in get_ui_id and not ui_control
	void visit(NodeFunctionCall& node) override;
//	void visit(NodeParamList& node) override;

private:
	NodeProgram* m_program = nullptr;
	NodeCallback* m_init_callback = nullptr;
	bool m_is_init_callback = false;
	DefinitionProvider* m_def_provider = nullptr;
	std::unordered_map<NodeAST *, std::unique_ptr<NodeStatement>> m_function_inlines;

	std::unique_ptr<NodeFunctionCall> wrap_in_get_ui_id(std::unique_ptr<NodeAST> variable);

	/// declared variables
	int m_variables_declared = 0;
	/// declared arrays
	int m_arrays_declared = 0;
};