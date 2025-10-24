//
// Created by Mathias Vatter on 05.09.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * @class TokenCounter
 * counts the amount of tokens in a given AST (bison tokens)
 */
class TokenCounter final : public ASTVisitor {
	int m_tokens = 0;
public:

	int get_tokens(NodeAST& node) {
		m_tokens = 0;
		node.accept(*this);
		return m_tokens;
	}

protected:
	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) {
			node.index->accept(*this);
			m_tokens += 1;
		}
		return &node;
	}

	NodeAST * visit(NodeArray& node) override {
		node.size->accept(*this);
		m_tokens += 1;
		return &node;
	}

	NodeAST * visit(NodeVariable& node) override {
		if (node.data_type == DataType::Const) m_tokens += 1;
		// m_tokens += 1;
		return &node;
	}
	//
	// NodeAST *visit(NodeVariableRef &node) override {
	// 	m_tokens += 1;
	// 	return &node;
	// }
	//
	// NodeAST* visit(NodeInt& node) override {
	// 	m_tokens += 1;
	// 	return &node;
	// }
	//
	// NodeAST* visit(NodeReal& node) override {
	// 	m_tokens += 1;
	// 	return &node;
	// }
	//
	// NodeAST* visit(NodeString& node) override {
	// 	m_tokens += 1;
	// 	return &node;
	// }

	NodeAST* visit(NodeBinaryExpr& node) override {
		// if its the first binary expression, add 2 token
		node.left->accept(*this);
		node.right->accept(*this);
		if(node.parent->get_node_type() != NodeType::BinaryExpr) {
			m_tokens += 2;
		}
		return &node;
	}

	NodeAST* visit(NodeUnaryExpr& node) override {
		// if its the first unary expression, add 1 token
		node.operand->accept(*this);
		// := -(1+1)
		if(node.parent->get_node_type() == NodeType::SingleAssignment or node.parent->get_node_type() == NodeType::SingleDeclaration) {
			m_tokens += 1;
		}
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		int func_tokens = node.function->get_num_args();
		func_tokens += 1; // because of function name
		m_tokens += func_tokens;
		if(StringUtils::contains(node.function->name, "nks")) {
			m_tokens += func_tokens;
		}
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment& node) override {
		node.l_value->accept(*this);
		node.r_value->accept(*this);
		m_tokens += 2;
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		m_tokens += 1; // because of variable name
		if(node.value) {
			node.value->accept(*this);
			m_tokens += 1;
		}
		return &node;
	}

	NodeAST* visit(NodeUIControl& node) override {
		node.control_var->accept(*this);
		node.params->accept(*this);
		m_tokens += 1; // because of ui_button keyword e.g.
		m_tokens += node.params->params.size();
		return &node;
	}

	NodeAST* visit(NodeIf& node) override {
		m_tokens += 5; // if, condition, then, if, end
		if(!node.else_body->statements.empty()) {
			m_tokens += 1;
		}
		return &node;
	}

	NodeAST* visit(NodeWhile& node) override {
		m_tokens += 5; // while, condition, while, end
		return &node;
	}

};