//
// Created by Mathias Vatter on 03.09.24.
//

#pragma once


#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * @class VarExistsValidator
 * checks if provided variable exists as reference in the AST
 */
class VarExistsValidator final : public ASTVisitor {
	bool m_exists = false;
	std::string m_var_name;

	// exception to break the visiting process early
	struct FoundVar final : std::exception {
		[[nodiscard]] const char* what() const noexcept override { return "found"; }
	};

public:
	explicit VarExistsValidator() = default;

	bool check(NodeAST& node, const std::string& var_name) {
		m_var_name = var_name;
		m_exists = false;
		try {
			node.accept(*this);
		} catch (const FoundVar&) {
			m_exists = true;
		}
		return m_exists;
	}

private:
	void compare_and_throw(const NodeReference& ref) {
		if(ref.name == m_var_name) {
			m_exists = true;
			throw FoundVar();
		}
	}

	NodeAST * visit(NodePointerRef& node) override {
		compare_and_throw(node);
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		compare_and_throw(node);
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		compare_and_throw(node);
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		compare_and_throw(node);
		return &node;
	}
};