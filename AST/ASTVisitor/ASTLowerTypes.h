//
// Created by Mathias Vatter on 19.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTLowerTypes: public ASTVisitor {
public:
	explicit ASTLowerTypes(DefinitionProvider *definition_provider): m_def_provider(definition_provider) {};

	inline NodeAST * visit(NodeVariable& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeVariableRef& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodePointer& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodePointerRef& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeList& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeListRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		return node.lower_type();
	}
	inline NodeAST * visit(NodeFunctionDefinition& node) override {
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			node.set_element_type(TypeRegistry::Integer);
		}

		node.header->accept(*this);
		node.body->accept(*this);

		return &node;
	}
	inline NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			node.set_element_type(TypeRegistry::Integer);
		}
		return &node;
	}

private:
	DefinitionProvider* m_def_provider;
};