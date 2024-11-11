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
 * Changes node types of function parameters at call sites regarding the function definition.
 * Changes node types of incorrectly detected data structures and references.
 */
class ASTSemanticAnalysis: public ASTVisitor {
public:
	explicit ASTSemanticAnalysis(DefinitionProvider* definition_provider);

    NodeAST * visit(NodeProgram& node) override;
	NodeAST * visit(NodeCallback& node) override;

	NodeAST * visit(NodeBlock& node) override;
	NodeAST * visit(NodeSingleAssignment& node) override;

	NodeAST * visit(NodeArray& node) override;
    NodeAST * visit(NodeArrayRef& node) override;

	NodeAST * visit(NodeNDArray& node) override;
    NodeAST * visit(NodeNDArrayRef& node) override;

	NodeAST * visit(NodePointer& node) override;
	NodeAST * visit(NodePointerRef& node) override;

	/// List Struct and Reference
	NodeAST * visit(NodeList& node) override;
	NodeAST * visit(NodeListRef& node) override;
	/// Variable
	NodeAST * visit(NodeVariable& node) override;
    NodeAST * visit(NodeVariableRef& node) override;

	NodeAST * visit(NodeFunctionHeaderRef& node) override;
	/// provide and search for function definition; handle is_thread_safe flag
	NodeAST * visit(NodeFunctionCall& node) override;
    /// add function parameters to scope
	NodeAST * visit(NodeFunctionDefinition& node) override;
	NodeAST * visit(NodeWildcard& node) override;
	NodeAST * visit(NodeNumElements& node) override;

	/// updates the node types of parameters at call sites regarding the function definition
	/// e.g. params can be incorrectly detected as variable refs at call sites, but they are arrays in the definition
	void update_func_call_node_types(NodeFunctionCall* func_call);
	/// updates incorrectly detected function params (eg arrays detected as variables)
	NodeDataStructure* replace_incorrectly_detected_data_struct(std::shared_ptr<NodeDataStructure> data_struct);
	/// updated incorrectly detected references of function params
	static NodeReference* replace_incorrectly_detected_reference(DefinitionProvider* def_provider, NodeReference* reference);

private:
    NodeProgram* m_program = nullptr;
	DefinitionProvider* m_def_provider = nullptr;

};

