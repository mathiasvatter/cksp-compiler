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
	void visit(NodeVariableRef& node) override;
	void visit(NodeVariable& node) override;
	void visit(NodeArray& node) override;
	void visit(NodeArrayRef& node) override;
//	void visit(NodeBinaryExpr& node) override;
//	void visit(NodeUnaryExpr& node) override;
//	void visit(NodeFunctionCall& node) override;
//	void visit(NodeFunctionHeader& node) override;
//	void visit(NodeBody& node) override;

private:
	DefinitionProvider* m_def_provider;

	/// tries to match the type of node 1 to the type of node2 after checking type compatibility
	static inline Type* match_type(NodeAST* node1, NodeAST* node2) {
		auto error = CompileError(ErrorType::TypeError,"", "", node1->tok);
		if(!node1->ty->is_compatible(node2->ty)) {
			error.m_message = "Type mismatch: " + node1->ty->to_string() + " and " + node2->ty->to_string();
			error.exit();
		}
		// get element types if composite type (element typ not nullptr):
		auto node_1 = node1->ty->get_element_type() ? node1->ty->get_element_type() : node1->ty;
		auto node_2 = node2->ty->get_element_type() ? node2->ty->get_element_type() : node2->ty;

		// specialize types:

		// if node 2 is unknown, return type of node1
		if (node_2 == TypeRegistry::Unknown) return node_1;
		// if node 1 is unknown, return type of node2
		if(node_1 == TypeRegistry::Unknown) {node_1 = node_2; return node_2;}

		// if node 1 is any, node2 has to be more specialized, return node_2
		if (node_1 == TypeRegistry::Any) {node_1 = node_2; return node_2;}
		// if node 2 is any, node1 has to be more specialized, return node_1
		if(node_2 == TypeRegistry::Any) return node_1;

		// if node 1 is number, node 2 has to be number, int or real, return node2
		if(node_1 == TypeRegistry::Number) {node_1 = node_2; return node_2;}
		// if node 2 is number, it is the other way around
		if(node_2 == TypeRegistry::Number) return node_1;

		// in all other cases, node_1 is already specialized
		return node_1;
	}
};