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

	void visit(NodeBody& node) override;
	void visit(NodeUIControl& node) override;

    void visit(NodeSingleDeclareStatement& node) override;

	void visit(NodeArray& node) override;
    void visit(NodeArrayRef& node) override;

	void visit(NodeNDArray& node) override;
    void visit(NodeNDArrayRef& node) override;

	/// List Struct and Reference
	void visit(NodeListStruct& node) override;
	void visit(NodeListStructRef& node) override;
	/// Variable
	void visit(NodeVariable& node) override;
    void visit(NodeVariableRef& node) override;

	/// provide and search for function definition; handle is_thread_safe flag
	void visit(NodeFunctionCall& node) override;
    /// add function parameters to scope
    void visit(NodeFunctionDefinition& node) override;

private:
    NodeProgram* m_program = nullptr;
    NodeBody* m_current_body = nullptr;
	DefinitionProvider* m_def_provider = nullptr;

	/// Checks for existence and uniqueness of "on init" callback
	/// If found, returns pointer to the callback node
	static NodeCallback* move_on_init_callback(NodeProgram& node);
	/// Checks for uniqueness of all callbacks except "on ui_control"
	static bool check_unique_callbacks(NodeProgram& node);

	/// updates the node types of parameters at call sites regarding the function definition
	/// e.g. args can be incorrectly detected as variable refs at call sites, but they are arrays in the definition
	static void update_func_call_node_types(NodeFunctionCall* func_call);

	/// Returns Function Definition from parameter
	inline NodeFunctionDefinition* get_function_definition_from_param(NodeDataStructure* param) {
		if(!param->is_function_param()) return nullptr;
		if(param->parent->get_node_type() != NodeType::ParamList) return nullptr;
		if(param->parent->parent->get_node_type() != NodeType::FunctionHeader) return nullptr;
		if(param->parent->parent->parent->get_node_type() == NodeType::FunctionDefinition) {
			return static_cast<NodeFunctionDefinition*>(param->parent->parent->parent);
		}
		return nullptr;
	}
};

