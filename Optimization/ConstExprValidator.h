//
// Created by Mathias Vatter on 18.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * @class ConstantExpressionAnalyzer
 * @brief A class to analyze AST nodes for constant expressions.
 *
 * The ConstantExpressionAnalyzer is designed to traverse specific nodes
 * within an Abstract Syntax Tree (AST) and determine whether an expression
 * is constant. This is useful in various compiler optimizations such as
 * constant propagation, where constant expressions are replaced with their
 * evaluated values at compile time.
 *
 * ### Key Responsibilities:
 * - Identifies if a given AST node represents a constant expression.
 * - Supports evaluation of number literals, constant variable references,
 *   and simple arithmetic expressions.
 * - Integrates with the DefinitionProvider to validate whether variables
 *   are constant.
 */
class ConstExprValidator : public ASTVisitor {
private:
	bool m_is_constant = true;

public:
	bool is_constant(NodeAST& node) {
		m_is_constant = true;
		node.accept(*this);
		return m_is_constant;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		if(node.data_type == DataType::Const) {
			m_is_constant &= true;
		} else {
			m_is_constant &= false;
		}
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		if(node.data_type == DataType::Const) {
			m_is_constant &= true;
		} else {
			m_is_constant &= false;
		}
		if(node.ty->get_type_kind() == TypeKind::Composite) {
			m_is_constant &= false;
		}
		return &node;
	}

	NodeAST * visit(NodeReal& node) override {
		m_is_constant &= true;
		return &node;
	}

	NodeAST * visit(NodeInt& node) override {
		m_is_constant &= true;
		return &node;
	}

	NodeAST * visit(NodeString& node) override {
		m_is_constant &= true;
		return &node;
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		// check if func is 'num_elements' which returns constant and can be used as array size
		if(node.kind == NodeFunctionCall::Kind::Builtin and node.function->args->params.size() == 1) {
			if(node.function->name == "num_elements" or node.function->name == "get_ui_id") {
				m_is_constant &= true;
				return &node;
			}
		}
		m_is_constant &= false;
		return &node;
	}
};