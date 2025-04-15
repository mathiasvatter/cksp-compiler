//
// Created by Mathias Vatter on 14.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

class ConstantPropagation final : public ASTVisitor {
	std::unordered_map<std::string, std::unique_ptr<NodeAST>> m_constants;
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

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if (const auto definition = node.get_definition()) {
			if (!definition->visited) definition->accept(*this);
			definition->visited = true;
		}
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) {
			node.value->accept(*this);
			// when declared as const -> replace with dead code node
			if(node.variable->data_type == DataType::Const and node.variable->get_node_type() == NodeType::Variable) {
				m_constants[node.variable->name] = std::move(node.value);
				return node.remove_node();
			}
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
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
		// do not substitute if the variable is on the left side of an assignment
		if(node->is_l_value()) return node;

		if (!m_constants.empty()) {
			if (auto substitute = get_constant(node->name)) {
				if (substitute->ty->is_compatible(node->ty)) {
					return node->replace_with(std::move(substitute));
				}
			}
		}
		return node;
	}

private:
	std::unique_ptr<NodeAST> get_constant(const std::string& name) {
		auto it = m_constants.find(name);
		if(it != m_constants.end()) {
			auto constant = it->second->clone();
			constant->update_parents(nullptr);
			return constant;
		}
		return nullptr;
	}


};