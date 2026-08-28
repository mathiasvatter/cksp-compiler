//
// Created by Mathias Vatter on 10.04.26.
//

#pragma once

#include <ranges>

#include "../ASTVisitor/ASTVisitor.h"

/**
 * In the given AST node, collects all variable references and offers methods to reason about them
 */
class VariableCollector final : public ASTVisitor {
	std::unordered_set<NodeReference*> m_references;
	std::vector<NodeReference*> m_reference_vec;
	std::vector<NodeReference*> m_const_references;
	std::vector<NodeReference*> m_non_const_references;
	std::unordered_set<std::string> m_reference_names;
	bool m_contains_local_references = false;
	bool m_do_visit_functions = true;
	std::unordered_set<NodeFunctionDefinition*> m_visited_functions;

	static bool is_declared_const(const NodeReference& ref) {
		if (const auto declaration = ref.get_declaration()) {
			return declaration->data_type == DataType::Const;
		}
		return ref.data_type == DataType::Const;
	}

	void add_reference(NodeReference& ref) {
		if (!m_references.insert(&ref).second) return;
		m_contains_local_references |= ref.is_local;
		m_reference_vec.push_back(&ref);
		if (is_declared_const(ref)) {
			m_const_references.push_back(&ref);
		} else {
			m_non_const_references.push_back(&ref);
		}
		m_reference_names.insert(ref.name);
	}

public:
	void collect(NodeAST &node, bool visit_functions = true) {
		m_contains_local_references = false;
		m_references.clear();
		m_reference_names.clear();
		m_reference_vec.clear();
		m_const_references.clear();
		m_non_const_references.clear();
		m_visited_functions.clear();
		m_do_visit_functions = visit_functions;
		node.accept(*this);
	}

	std::unordered_set<NodeFunctionDefinition*>& get_visited_functions() {
		return m_visited_functions;
	}

	bool contains_local_references() const {
		return m_contains_local_references;
	}

	std::vector<NodeReference*>& get_reference_vec() {
		return m_reference_vec;
	}

	const std::vector<NodeReference*>& get_const_references() const {
		return m_const_references;
	}

	const std::vector<NodeReference*>& get_non_const_references() const {
		return m_non_const_references;
	}

private:

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if (!m_do_visit_functions) return &node;

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

	NodeAST* visit(NodeListRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		add_reference(node);
		return &node;
	}



};
