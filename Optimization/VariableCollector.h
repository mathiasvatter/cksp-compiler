//
// Created by Mathias Vatter on 10.04.26.
//

#pragma once

#include <ranges>

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * In the given AST node, collects all variable references and offers methods to reason about them
 */
class VariableCollector final : public ASTVisitor {
	std::unordered_set<NodeReference*> m_references;
	std::vector<NodeReference*> m_reference_vec;
	std::unordered_set<std::string> m_reference_names;
	bool m_contains_local_references = false;
	std::unordered_set<NodeFunctionDefinition*> m_visited_functions;

	void add_reference(NodeReference& ref) {
		m_contains_local_references |= ref.is_local;
		m_references.insert(&ref);
		m_reference_vec.push_back(&ref);
		m_reference_names.insert(ref.name);
	}

public:
	void collect(NodeAST &node) {
		m_contains_local_references = false;
		m_references.clear();
		m_reference_names.clear();
		m_reference_vec.clear();
		m_visited_functions.clear();
		node.accept(*this);
	}

	std::unordered_set<NodeFunctionDefinition*> get_visited_functions() {
		return std::move(m_visited_functions);
	}

	bool contains_local_references() const {
		return m_contains_local_references;
	}

private:

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		const auto definition = node.get_definition();
		if (definition) {
			if (!m_visited_functions.contains(definition.get())) {
				definition->accept(*this);
				m_visited_functions.insert(definition.get());
			}
		}
		return &node;
	}

	NodeAST *visit(NodePointerRef &node) override {
		add_reference(node);
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		add_reference(node);
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		add_reference(node);
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		add_reference(node);
		return &node;
	}



};