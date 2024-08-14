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
		if(node.value) node.value->accept(*this);
		if(node.variable->data_type == DataType::Const and node.value) {
			m_constants[node.variable->name] = std::move(node.value);
			node.variable->is_used = false;
			return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		return do_propagation(&node);
	}

	std::unique_ptr<NodeAST> get_constant(const std::string& name) {
		auto it = m_constants.find(name);
		if(it != m_constants.end()) {
			auto constant = it->second->clone();
			constant->update_parents(nullptr);
			return constant;
		}
		return nullptr;
	}

	NodeAST* do_propagation(NodeReference* node) {
		if(!m_constants.empty()) {
			if(auto substitute = get_constant(node->name)) {
				return node->replace_with(std::move(substitute));
			}
		}
		return node;
	}

};