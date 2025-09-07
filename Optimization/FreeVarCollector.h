//
// Created by Mathias Vatter on 23.08.25.
//

#pragma once

#include <ranges>

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * In the given AST node, collects all free variables that are not
 * defined in the node's scope.
 */
class FreeVarCollector final : public ASTVisitor {
	std::unordered_set<NodeReference*> m_free_vars;
	std::vector<std::unordered_set<std::string>> m_bound_var_decls;
	// names of free variables can only be unique
	std::unordered_set<std::string> m_free_var_names;
	std::unordered_set<NodeFunctionDefinition*> m_visited_functions;

	bool is_bound_var(const NodeReference& ref) {
		// search from innermost to outermost scope
		for (auto & m_bound_var_decl : std::ranges::reverse_view(m_bound_var_decls)) {
			return m_bound_var_decl.contains(ref.name);
		}
		return false;
	}

public:
	std::unordered_set<std::string> collect(NodeAST &node) {
		m_free_vars.clear();
		m_free_var_names.clear();
		m_bound_var_decls.clear();
		m_visited_functions.clear();
		m_bound_var_decls.emplace_back(); // global scope
		node.accept(*this);
		m_bound_var_decls.clear();
		return std::move(m_free_var_names);
	}

	std::unordered_set<NodeFunctionDefinition*> get_visited_functions() {
		return std::move(m_visited_functions);
	}

private:
	NodeAST *visit(NodeBlock &node) override {
		if (node.scope) {
			m_bound_var_decls.emplace_back();
		}
		visit_all(node.statements, *this);
		if (node.scope) {
			m_bound_var_decls.pop_back();
		}
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		auto definition = node.get_definition();
		if (definition) {
			if (!m_visited_functions.contains(definition.get())) {
				definition->accept(*this);
				m_visited_functions.insert(definition.get());
			}
		}
		return &node;
	}

	NodeAST *visit(NodeFunctionDefinition &node) override {
		m_bound_var_decls.emplace_back();
		ASTVisitor::visit(node);
		m_bound_var_decls.pop_back();
		return &node;
	}


	// all declarations are bound variables
	NodeAST * visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		m_bound_var_decls.back().insert(node.variable->name);
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST * visit(NodeFunctionParam& node) override {
		node.variable->accept(*this);
		m_bound_var_decls.back().insert(node.variable->name);
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST * visit(NodePointerRef& node) override {
		if(!is_bound_var(node)) {
			m_free_var_names.insert(node.name);
		}
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		if(!is_bound_var(node)) {
			m_free_var_names.insert(node.name);
		}
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		if(!is_bound_var(node)) {
			m_free_var_names.insert(node.name);
		}
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(!is_bound_var(node)) {
			m_free_var_names.insert(node.name);
		}
		return &node;
	}



};