//
// Created by Mathias Vatter on 14.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../AST/ASTVisitor/ASTOptimizations.h"

class ConstantPropagation final : public ASTOptimizations {
	std::unordered_map<StringTypeKey, std::unique_ptr<NodeAST>, StringTypeKeyHash> m_constants;

	std::mutex m_constants_mutex;

	void add_constant(NodeDataStructure& data, const NodeAST* value) {
		if(data.data_type == DataType::Const) {
			m_constants[get_hash_value(data)] = value->clone();
		}
	}
	void add_constant(NodeArrayRef& ref, const NodeAST* value) {
		if(ref.data_type == DataType::Const) {
			m_constants[get_hash_value(ref)] = value->clone();
		}
	}

	std::unique_ptr<NodeAST> get_constant(NodeAST& node) {
		auto it = m_constants.find(get_hash_value(node));
		if(it != m_constants.end()) {
			auto constant = it->second->clone();
			return constant;
		}
		return nullptr;
	}

public:
	// this seems to cause segfault errors under certain circumstances... -> switched back to one thread
	// segfault maybe originates from NodeReference destructor that accesses reference set of connected
	// NodeDataStructure
	NodeAST *do_parallel_traversal(NodeProgram &node) {
		m_program = &node;
		m_constants.clear();
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		parallel_for_each(node.callbacks.begin(), node.callbacks.end(),
			[this](const auto & callback) {
          		if (callback.get() == m_program->init_callback) return;
				callback->accept(*this);
			}
		);
		parallel_for_each(node.function_definitions.begin(), node.function_definitions.end(),
			[this](const auto & func) {
				func->accept(*this);
			}
		);

		node.reset_function_visited_flag();
		return &node;
	}

	std::unordered_map<StringTypeKey, std::unique_ptr<NodeAST>, StringTypeKeyHash> get_constants() {
		return std::move(m_constants);
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) {
			node.value->accept(*this);
			// when declared as const -> replace with dead code node
			if(node.variable->data_type == DataType::Const) {
				if (node.variable->cast<NodeVariable>()) {
					add_constant(*node.variable, node.value.get());
					return node.remove_node();
				} else if (auto node_array = node.variable->cast<NodeArray>()) {
					if (!node_array->size) return &node;
					// split the const array up into its references
					node_array->size->do_constant_folding();
					if (auto size = node_array->get_size()) {
						if (auto s = size->cast<NodeInt>()) {
							auto init_list = node.value->cast<NodeInitializerList>();
							if (!init_list) return &node;
							auto node_array_ref = unique_ptr_cast<NodeArrayRef>(node.variable->to_reference());
							node_array_ref->ty = node_array->ty->get_element_type();
							for (int i = 0; i < s->value; ++i) {
								node_array_ref->set_index(std::make_unique<NodeInt>(i, Token()));
								add_constant(*node_array_ref, init_list->elem(i).get());
							}
						}
					}
				}
			}
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		return do_constant_propagation(&node);
	}

	NodeAST* visit(NodeArrayRef& node) override {
		ASTVisitor::visit(node);
		return do_constant_propagation(&node);
	}

	NodeAST* visit(NodeGetControl& node) override {
		auto error = CompileError(ErrorType::InternalError, "GetControl node should exist anymore in ConstantPropagation", "", node.tok);
		error.exit();
		return &node;
	}

	NodeAST* visit(NodeSetControl& node) override {
		auto error = CompileError(ErrorType::InternalError, "SetControl node should exist anymore in ConstantPropagation", "", node.tok);
		error.exit();
		return &node;
	}

	// to constant propagation (with declared constants) everywhere except left hand of assignments
	NodeAST* do_constant_propagation(NodeReference* node) {
		if (node->data_type != DataType::Const) return node;
		// if builtin return ????????? TODO: what does this mean?
		// if (node->is_engine) return node;
		// do not substitute if the variable is on the left side of an assignment
		if(node->is_l_value()) return node;

		if (!m_constants.empty()) {
			if (auto substitute = get_constant(*node)) {
				if (substitute->ty->is_compatible(node->ty)) {
					{
						// std::lock_guard<std::mutex> lock(m_constants_mutex);
						return node->replace_with(std::move(substitute));
					}
				}
			}
		}
		return node;
	}

};