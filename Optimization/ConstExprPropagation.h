//
// Created by Mathias Vatter on 14.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"

class ConstExprPropagation final : public ASTOptimizations {
	std::unordered_map<StringTypeKey, std::unique_ptr<NodeAST>, StringTypeKeyHash> m_constant_expressions;

public:

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	/// reset the constant expression propagation map every block
	NodeAST* visit(NodeBlock& node) override {
		m_constant_expressions.clear();
		for(const auto & stmt : node.statements) {
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
		// if function is builtin and asynchronus, do not propagate -> reset map
		if(node.kind == NodeFunctionCall::Kind::Builtin) {
			if(!node.get_definition()->is_thread_safe) {
				m_constant_expressions.clear();
			}
		} else {
			if (const auto definition = node.get_definition()) {
				if (!definition->visited) definition->accept(*this);
				definition->visited = true;
			}
		}
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment& node) override {
		node.r_value->accept(*this);
		// remove constant from constant expression map when it gets reassigned
		remove_constant_expression(node.l_value.get());
		node.l_value->accept(*this);
		// if mutable, try to propagate the value
		if(node.l_value->data_type == DataType::Mutable) {
			if(node.r_value->is_constant()) {
				m_constant_expressions[get_hash_value(*node.l_value)] = node.r_value->clone();
			}
		}

		return &node;
	}

	bool remove_constant_expression(NodeReference* node) {
		auto const it = m_constant_expressions.find(get_hash_value(*node));
		if(it != m_constant_expressions.end()) {
			m_constant_expressions.erase(it);
			return true;
		}
		return false;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if(is_destructive_func_arg(&node)) {
			remove_constant_expression(&node);
		}
		return do_constant_expr_propagation(&node);
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(is_destructive_func_arg(&node)) {
			remove_constant_expression(&node);
		}
		return do_constant_expr_propagation(&node);
	}

	NodeAST* do_constant_expr_propagation(NodeReference* node) {
		// do not substitute if the variable is on the left side of an assignment
		if(node->is_l_value()) return node;
		/// propagation inside certain builtin functions is not allowed
		/// if parameter in builtin function is a variable or an array -> no propagation
		if(node->is_func_arg()) {
			auto func_call = node->parent->parent->parent->cast<NodeFunctionCall>();
			if(func_call and func_call->kind == NodeFunctionCall::Kind::Builtin) {
				if(func_call->get_definition()) {
					auto param_list = node->parent->cast<NodeParamList>();
					auto &param = func_call->get_definition()->header->get_param(param_list->get_idx(node));
					if(contains(param->name, "variable")) {
						return node;
					}
					if(contains(param->name, "array")) {
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
		const auto it = m_constant_expressions.find(get_hash_value(*node));
		if(it != m_constant_expressions.end()) {
			auto constant = it->second->clone();
			constant->update_parents(nullptr);
			return constant;
		}
		return nullptr;
	}

};