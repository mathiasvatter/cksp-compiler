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
class DeadCodeElimination final : public ASTOptimizations {

	std::unordered_map<StringTypeKey, NodeReference*, StringTypeKeyHash> m_last_reference;

public:

	DeadCodeElimination() {
		m_last_reference.clear();
	}

	/// kill empty while loops and if statements
	NodeAST* visit(NodeWhile& node) override {
		node.condition->accept(*this);
		node.body->accept(*this);

		if(node.body->statements.empty()) {
			m_last_reference.clear();
			return node.remove_node();
		}

		/// if condition is false, remove while loop, if true, replace with 1=1
		if(auto node_int = node.condition->cast<NodeInt>()) {
			if(node_int->value == 1) {
				auto true_expr = std::make_unique<NodeBinaryExpr>(token::EQUAL, std::make_unique<NodeInt>(1, node.tok), std::make_unique<NodeInt>(1, node.tok), node.tok);
				true_expr->ty = TypeRegistry::Boolean;
				m_last_reference.clear();
				return node.condition->replace_with(std::move(true_expr));
			} else if(node_int->value == 0) {
				m_last_reference.clear();
				return node.remove_node();
			}
		}

		return &node;
	}

	NodeAST* visit(NodeIf& node) override {
		node.condition->accept(*this);
		node.if_body->accept(*this);
		node.else_body->accept(*this);

		// deal with empty if and/or else body
		if(node.if_body->empty() && node.else_body->empty()) {
			return node.remove_node();
		}
		// only else body left
		if (node.if_body->empty()) {
			node.set_if_body(std::move(node.else_body));
			node.set_else_body(std::make_unique<NodeBlock>(node.tok));

			// negate condition
			auto negated_condition = std::make_unique<NodeUnaryExpr>(token::BOOL_NOT, std::move(node.condition), node.tok);
			negated_condition->ty = TypeRegistry::Boolean;
			node.set_condition(std::move(negated_condition));
			node.condition->do_constant_folding();
		}

		// if condition is constant, remove one of the branches
		if(const auto node_int = node.condition->cast<NodeInt>()) {
			if(node_int->value == 1) {
				m_last_reference.clear();
				return node.replace_with(std::move(node.if_body));
			}
			if(node_int->value == 0) {
				if(node.else_body->empty()) {
					m_last_reference.clear();
					return node.remove_node();
				}
				m_last_reference.clear();
				return node.replace_with(std::move(node.else_body));
			}
		}

		return &node;
	}

	/// reset map every block
	NodeAST* visit(NodeBlock& node) override {
		m_last_reference.clear();
		for(const auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	};

	NodeAST* visit(NodeSingleAssignment& node) override {
		// important to do r_value first to remove last assignment if necessary
		node.r_value->accept(*this);
		node.l_value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		kill_last_assignment(&node);
		if(node.is_l_value()) m_last_reference[get_hash_value(node)] = &node;
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		kill_last_assignment(&node);
		if(node.index) node.index->accept(*this);
		// only store last reference if it has a constant index that will not change
		if(node.index and node.index->is_constant()) {
			if(node.is_l_value()) m_last_reference[get_hash_value(node)] = &node;
		}
		return &node;
	}

	/// if this and the last occurence of the var ref are assignment statements with this var
	/// on the left side, remove the last assignment
	/// TODO: do not kill last assignment is current assignment has the var as r_value
	/// var := 1*23
	/// var := var + 1 -> do not kill
	/// do not kill if r_value is function call (builtin or user defined)
	bool kill_last_assignment(NodeReference* node) {
		// only stay in here if current node is left side of assign statement
		if(!node->is_l_value()) return false;
		// check if last reference is an assignment statement
		if(m_last_reference.empty()) return false;
		auto const it = m_last_reference.find(get_hash_value(*node));
		if(it != m_last_reference.end()) {
			// check if reference is also somewhere on the right side of the assignment
			if(const auto assignment = it->second->is_l_value()) { //and !node->is_r_value()) {
				if (assignment->r_value->cast<NodeFunctionCall>()) return false;
				assignment->remove_node();
				m_last_reference.erase(it);
				return true;
			}
		}
		return false;
	}


};