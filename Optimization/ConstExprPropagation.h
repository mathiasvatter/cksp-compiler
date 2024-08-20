//
// Created by Mathias Vatter on 14.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"

class ConstExprPropagation : public ASTOptimizations {
private:
	std::unordered_map<StringTypeKey, std::unique_ptr<NodeAST>, StringTypeKeyHash> m_constant_expressions;

	/// map value will be reset after ref is in one of these functions
	inline static const std::unordered_set<std::string> no_propagation = {
		"inc", "dec",
	};
public:

	/// reset the constant expression propagation map every block
	NodeAST* visit(NodeBlock& node) override {
		m_constant_expressions.clear();
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		m_constant_expressions.clear();
		return &node;
	};

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) {
			node.value->accept(*this);
			// if mutable, try to propagate the value
			if(node.variable->data_type == DataType::Mutable and node.variable->get_node_type() == NodeType::Variable) {
				if(node.value->is_constant()) {
					m_constant_expressions[get_hash_value(*node.variable)] = node.value->clone();
				}
			}
		}

		return &node;
	}

	/// do not propagate in while loop condition
	NodeAST* visit(NodeWhile& node) override {
		node.body->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment& node) override {
		node.r_value->accept(*this);
		// remove constant from constant expression map when it gets reassigned
		auto ref = static_cast<NodeReference*>(node.l_value.get());
		remove_constant_expression(ref);
		node.l_value->accept(*this);
		// if mutable, try to propagate the value
		if(ref->data_type == DataType::Mutable) {
			if(node.r_value->is_constant()) {
				m_constant_expressions[get_hash_value(*node.l_value)] = node.r_value->clone();
			}
		}

		return &node;
	}

	bool remove_constant_expression(NodeReference* node) {
		auto it = m_constant_expressions.find(get_hash_value(*node));
		if(it != m_constant_expressions.end()) {
			m_constant_expressions.erase(it);
			return true;
		}
		return false;
	}

	static bool is_forbidden_func_arg(NodeReference* node) {
		if(node->is_func_arg()) {
			auto func_call = static_cast<NodeFunctionCall*>(node->parent->parent->parent);
			if(func_call->kind == NodeFunctionCall::Kind::Builtin) {
				if(contains(no_propagation, func_call->function->name)) {
					return true;
				}
			} else if (func_call->kind == NodeFunctionCall::Kind::UserDefined) {
				return true;
			}
		}
		return false;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if(is_forbidden_func_arg(&node)) {
			remove_constant_expression(&node);
		}
		return do_constant_expr_propagation(&node);
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(is_forbidden_func_arg(&node)) {
			remove_constant_expression(&node);
		}
		return do_constant_expr_propagation(&node);
	}

	NodeAST* do_constant_expr_propagation(NodeReference* node) {
		// do not substitute if the variable is on the left side of an assignment
		if(node->parent->get_node_type() == NodeType::SingleAssignment) {
			auto assignment = static_cast<NodeSingleAssignment*>(node->parent);
			if(assignment->l_value.get() == node) {
				return node;
			}
		}
		/// propagation inside certain builtin functions is not allowed
		/// if parameter in builtin function is a variable or an array -> no propagation
		if(node->is_func_arg()) {
			auto func_call = static_cast<NodeFunctionCall*>(node->parent->parent->parent);
			if(func_call->kind == NodeFunctionCall::Kind::Builtin) {
				if(func_call->definition) {
					auto param_list = static_cast<NodeParamList*>(node->parent);
					auto param = func_call->definition->header->args->params[param_list->get_idx(node)].get();
					if(contains(static_cast<NodeDataStructure*>(param)->name, "variable")) {
						return node;
					}
					if(contains(static_cast<NodeDataStructure*>(param)->name, "array")) {
						return node;
					}
				} else {
					return node;
				}
			} else {
				return node;
			}
		}

		if(!m_constant_expressions.empty()) {
			if(auto substitute = get_constant_expression(node)) {
				if(substitute->ty->is_compatible(node->ty)) {
					return node->replace_with(std::move(substitute));
				}
			}
		}
		return node;
	}



private:

	std::unique_ptr<NodeAST> get_constant_expression(NodeAST* node) {
		auto it = m_constant_expressions.find(get_hash_value(*node));
		if(it != m_constant_expressions.end()) {
			auto constant = it->second->clone();
			constant->update_parents(nullptr);
			return constant;
		}
		return nullptr;
	}

};