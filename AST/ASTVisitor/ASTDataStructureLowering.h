//
// Created by Mathias Vatter on 30.07.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTDataStructureLowering: public ASTVisitor {
private:
	DefinitionProvider* m_def_provider;
public:
	explicit ASTDataStructureLowering(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {};

	inline NodeAST* visit(NodeSingleDeclaration &node) override {
//		node.variable->accept(*this);
		if(node.value) node.value ->accept(*this);
		return node.lower(m_program);
	}

	inline NodeAST* visit(NodeSingleAssignment &node) override {
//		node.r_value ->accept(*this);
		return node.lower(m_program);
	}

	inline NodeAST * visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		return node.lower(m_program);
	}

	inline NodeAST * visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		return node.lower(m_program);
	}

	inline NodeAST * visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		return node.lower(m_program);
	}
};