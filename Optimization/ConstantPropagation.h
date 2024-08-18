//
// Created by Mathias Vatter on 14.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

class ConstantPropagation : public ASTVisitor {
private:
	std::unordered_map<std::string, std::unique_ptr<NodeAST>> m_constants;
public:

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) {
			node.value->accept(*this);
			// when declared as const -> replace with dead code node
			if(node.variable->data_type == DataType::Const) {
				m_constants[node.variable->name] = std::move(node.value);
				return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
			}
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		return do_constant_propagation(&node);
	}

	// to constant propagation (with declared constants) everywhere except left hand of assignments
	NodeAST* do_constant_propagation(NodeReference* node) {
		if (node->data_type != DataType::Const) return node;
		// do not substitute if the variable is on the left side of an assignment
		if (node->parent->get_node_type() == NodeType::SingleAssignment) {
			auto assignment = static_cast<NodeSingleAssignment *>(node->parent);
			if (assignment->l_value.get() == node) {
				return node;
			}
		}
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