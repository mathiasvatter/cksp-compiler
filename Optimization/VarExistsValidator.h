//
// Created by Mathias Vatter on 03.09.24.
//

#pragma once


#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * @class VarExistsValidator
 * checks if provided variable exists as reference in the AST
 */
class VarExistsValidator : public ASTVisitor {
private:
	bool m_exists = false;
	std::string m_var_name;
public:
	explicit VarExistsValidator() = default;

	bool var_exists(NodeAST& node, const std::string& var_name) {
		m_var_name = var_name;
		m_exists = false;
		node.accept(*this);
		return m_exists;
	}

private:

	NodeAST * visit(NodeVariableRef& node) override {
		if(node.name == m_var_name) {
			m_exists = true;
		}
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		if(node.name == m_var_name) {
			m_exists = true;
		}
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.name == m_var_name) {
			m_exists = true;
		}
		return &node;
	}
};