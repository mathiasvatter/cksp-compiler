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

	inline void visit(NodeVariable& node) override {
		node.lower_type();
	}
	inline void visit(NodeVariableRef& node) override {
		node.lower_type();
	}
	inline void visit(NodePointer& node) override {
		node.lower_type();
	}
	inline void visit(NodePointerRef& node) override {
		node.lower_type();
	}
	inline void visit(NodeArray& node) override {
		node.lower_type();
	}
	inline void visit(NodeArrayRef& node) override {
		node.lower_type();
	}
	inline void visit(NodeNDArray& node) override {
		node.lower_type();
	}
	inline void visit(NodeNDArrayRef& node) override {
		node.lower_type();
	}
	inline void visit(NodeList& node) override {
		node.lower_type();
	}
	inline void visit(NodeListRef& node) override {
		node.lower_type();
	}
	inline void visit(NodeFunctionDefinition& node) override {
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			node.set_element_type(TypeRegistry::Integer);
		}
		node.header->accept(*this);
		node.body->accept(*this);
	}
	inline void visit(NodeFunctionCall& node) override {
		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
			node.set_element_type(TypeRegistry::Integer);
		}
		node.function->accept(*this);
	}


};