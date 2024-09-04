//
// Created by Mathias Vatter on 19.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTLowerTypes: public ASTVisitor {
public:
	explicit ASTLowerTypes(DefinitionProvider *definition_provider) {};

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
		return node.lower_type();
	}
	inline NodeAST * visit(NodeArrayRef& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArray& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeNDArrayRef& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeList& node) override {
		return node.lower_type();
	}
	inline NodeAST * visit(NodeListRef& node) override {
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
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			node.set_element_type(TypeRegistry::Integer);
		}
		node.function->accept(*this);
		return &node;
	}

};