//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

/**
 * This class completes ASTNodes like arrays, UI controls, etc., by filling in declaration information.
 * It also checks for uniqueness of callback names and existence of "on init" callback.
 * It adds globally declared data structures to the start of the program.
 * It tracks data structure definitions with DefinitionProvider and sets scopes for body nodes.
 * It adds scope to the following:
 * - if-statement
 * - select-statement
 * - while-statement
 * - function definition
 * - callback
 */
class ASTBuildDataStructures: public ASTVisitor {
public:
	explicit ASTBuildDataStructures(DefinitionProvider* definition_provider);

    void visit(NodeProgram& node) override;
	void visit(NodeCallback& node) override;

	void visit(NodeBlock& node) override;

	void visit(NodeArray& node) override;
    void visit(NodeArrayRef& node) override;

	void visit(NodeNDArray& node) override;
    void visit(NodeNDArrayRef& node) override;

	void visit(NodePointer& node) override;
	void visit(NodePointerRef& node) override;

	/// List Struct and Reference
	void visit(NodeList& node) override;
	void visit(NodeListRef& node) override;
	/// Variable
	void visit(NodeVariable& node) override;
    void visit(NodeVariableRef& node) override;

	/// provide and search for function definition; handle is_thread_safe flag
	void visit(NodeFunctionCall& node) override;
    /// add function parameters to scope
    void visit(NodeFunctionDefinition& node) override;

private:
    NodeProgram* m_program = nullptr;
	DefinitionProvider* m_def_provider = nullptr;

	/// updates the node types of parameters at call sites regarding the function definition
	/// e.g. args can be incorrectly detected as variable refs at call sites, but they are arrays in the definition
	void update_func_call_node_types(NodeFunctionCall* func_call);
	/// updates incorrectly detected function params (eg arrays detected as variables)
	void replace_incorrectly_detected_data_struct(NodeDataStructure* data_struct);
	/// updated incorrectly detected references of function params
	void replace_incorrectly_detected_reference(NodeReference* reference);

};

