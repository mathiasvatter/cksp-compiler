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
 * ### StringTypeKey Responsibilities:
 * - Identifies if a given AST node represents a constant expression.
 * - Supports evaluation of number literals, constant variable references,
 *   and simple arithmetic expressions.
 * - Integrates with the DefinitionProvider to validate whether variables
 *   are constant.
 */
class ConstExprValidator final : public ASTVisitor {
	bool m_is_constant = true;
	bool all_builtins_are_constant = false;
	bool arrayref_can_be_constants = true; // true by default, but Kontakt does not accept const custom arrays so has to be able to set to false sometimes
public:
	bool is_constant(NodeAST& node, bool builtins_are_const = false, bool arrayref_can_be_const = true) {
		m_is_constant = true;
		all_builtins_are_constant = builtins_are_const;
		arrayref_can_be_constants = arrayref_can_be_const;
		node.accept(*this);
		return m_is_constant;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		// if (node.is_local) {
		// 	m_is_constant &= false;
		// 	return &node;
		// }
		if(node.data_type == DataType::Const) {
			m_is_constant &= true;
		} else {
			if (all_builtins_are_constant and node.kind == NodeReference::Kind::Builtin) {
				m_is_constant &= true;
			} else {
				m_is_constant &= false;
			}
		}
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		// if (node.is_local) {
		// 	m_is_constant &= false;
		// 	return &node;
		// }
		if (arrayref_can_be_constants) {
			if(node.data_type == DataType::Const) {
				m_is_constant &= true;
				return &node;
			}
		}
		if (all_builtins_are_constant) {
			if (node.kind == NodeReference::Kind::Builtin) {
				m_is_constant &= true;
			}
		}
		// if it is of type composite it references the size -> which is constant
		if(node.ty->get_type_kind() == TypeKind::Composite) {
			m_is_constant &= true;
		} else {
			m_is_constant &= false;
		}
		return &node;
	}

	NodeAST * visit(NodeNumElements& node) override {
		node.array->accept(*this);
		if(node.dimension) node.dimension->accept(*this);
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
		const std::unordered_set<std::string> constant_functions = {
			"num_elements",
			"get_ui_id",
			"real",
			"int",
			"real_to_int",
			"int_to_real",
			"abs",
		};
		node.function->accept(*this);
		// check if func is 'num_elements' which returns constant and can be used as array size
		if(node.kind == NodeFunctionCall::Kind::Builtin and node.function->get_num_args() == 1) {
			if(constant_functions.contains(node.function->name)) {
				m_is_constant &= true;
				return &node;
			}
		} else if (node.kind == NodeFunctionCall::Kind::UserDefined) { // and node.function->get_num_args() == 2) {
			// if (node.function->name == "Array::clip") {
			// 	m_is_constant &= true;
			// 	return &node;
			// }
			if (auto def = node.get_definition()) {
				// Array::clip header is made const
				if (def->header->data_type == DataType::Const) {
					m_is_constant &= true;
					return &node;
				}
			}
		}
		m_is_constant &= false;
		return &node;
	}
};