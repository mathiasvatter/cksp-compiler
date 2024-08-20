//
// Created by Mathias Vatter on 18.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"
#include "ConstantFolding.h"

/**
 * Removes Dead Code:
 * - Assignments that are reassigned without being used
 */
class DeadCodeElimination : public ASTOptimizations {
private:
	std::unordered_map<StringTypeKey, NodeReference*, StringTypeKeyHash> m_last_reference;
public:

	/// kill empty while loops and if statements
	NodeAST* visit(NodeWhile& node) override {
		node.condition->accept(*this);
		node.body->accept(*this);

		if(node.body->statements.empty()) {
			return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		}

		/// if condition is false, remove while loop, if true, replace with 1=1
		if(node.condition->get_node_type() == NodeType::Int) {
			auto int_node = static_cast<NodeInt*>(node.condition.get());
			if(int_node->value == 1) {
				auto true_expr = std::make_unique<NodeBinaryExpr>(token::EQUAL, std::make_unique<NodeInt>(1, node.tok), std::make_unique<NodeInt>(1, node.tok), node.tok);
				true_expr->ty = TypeRegistry::Boolean;
				return node.condition->replace_with(std::move(true_expr));
			} else if(int_node->value == 0) {
				return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
			}
		}

		return &node;
	}

	NodeAST* visit(NodeIf& node) override {
		node.condition->accept(*this);
		node.if_body->accept(*this);
		node.else_body->accept(*this);

		// deal with empty if and/or else body
		if(node.if_body->statements.empty() && node.else_body->statements.empty()) {
			return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		// only else body left
		} else if (node.if_body->statements.empty()) {
			node.if_body = std::move(node.else_body);
			node.if_body->parent = &node;
			node.else_body = std::make_unique<NodeBlock>(node.tok);
			node.else_body->parent = &node;
			auto negated_condition = std::make_unique<NodeUnaryExpr>(token::BOOL_NOT, std::move(node.condition), node.tok);
			negated_condition->ty = TypeRegistry::Boolean;
			node.condition = std::move(negated_condition);
			node.condition->parent = &node;
			static ConstantFolding cf;
			node.condition->accept(cf);
		}

		// if condition is constant, remove one of the branches
		if(node.condition->get_node_type() == NodeType::Int) {
			auto int_node = static_cast<NodeInt*>(node.condition.get());
			if(int_node->value == 1) {
				return node.replace_with(std::move(node.if_body));
			} else if(int_node->value == 0) {
				if(node.else_body->statements.empty()) {
					return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
				} else {
					return node.replace_with(std::move(node.else_body));
				}
			}
		}

		return &node;
	}

	/// reset map every block
	NodeAST* visit(NodeBlock& node) override {
		m_last_reference.clear();
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	};

	NodeAST* visit(NodeVariableRef& node) override {
		kill_last_assignment(&node);
		m_last_reference[get_hash_value(node)] = &node;
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		kill_last_assignment(&node);
		if(node.index) node.index->accept(*this);
		m_last_reference[get_hash_value(node)] = &node;
		return &node;
	}

	/// if this and the last occurence of the var ref are assignment statements with this var
	/// on the left side, remove the last assignment
	bool kill_last_assignment(NodeReference* node) {
		// only stay in here if current node is left side of assign statement
		if(!node->is_l_value()) return false;
		// check if last reference is an assignment statement
		if(m_last_reference.empty()) return false;
		auto it = m_last_reference.find(get_hash_value(*node));
		if(it != m_last_reference.end()) {
			if(it->second->is_l_value()) {
				auto assignment = it->second->parent;
				assignment->replace_with(std::make_unique<NodeDeadCode>(assignment->tok));
				m_last_reference.erase(it);
				return true;
			}
		}
		return false;
	}

};