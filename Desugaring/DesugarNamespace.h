//
// Created by Mathias Vatter on 10.07.25.
//

#pragma once


#include "ASTDesugaring.h"
#include "../AST/ASTVisitor/ASTNoVisitor.h"

/**
 * Prepending prefix to variables and references that were declared in namespace
 */
class DesugarNamespace final : public ASTDesugaring {
	std::string prefix;
	std::unordered_set<std::string> m_namespace_variables;
	void add_namespace_prefix(NodeDataStructure& var) {
		m_namespace_variables.insert(var.name);
		var.name = prefix + "." + var.name;
	}
	void add_namespace_prefix(NodeReference& ref) const {
		if (m_namespace_variables.contains(ref.name)) {
			ref.name = prefix + "." + ref.name;
		}
	}
public:
	explicit DesugarNamespace(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST * visit(NodeNamespace& node) override {
		prefix = node.prefix;
		node.members->accept(*this);
		for(const auto & m: node.function_definitions) {
			m->accept(*this);
		}
		m_namespace_variables.clear();
		return &node;
	}

	// function parameter do not need namespace prefix
	NodeAST * visit(NodeFunctionParam& node) override {
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST * visit(NodeVariable& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodePointer& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodePointerRef& node) override {
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST* visit(NodeFunctionHeader& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST* visit(NodeFunctionHeaderRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeArray& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeNDArray& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeNDArrayRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeList& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeListRef& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

	NodeAST * visit(NodeConst& node) override {
		ASTVisitor::visit(node);
		add_namespace_prefix(node);
		return &node;
	}

};

class UnnestNamespaces final : public ASTNoVisitor {
	std::string m_prefixes;
	void add_prefix(const std::string& prefix) {
		if (!m_prefixes.empty()) {
			m_prefixes += ".";
		}
		m_prefixes += prefix;
	}
	std::stack<NodeNamespace*> m_stack;
public:
	explicit UnnestNamespaces(NodeProgram* program) {
		m_program = program;
	}

	NodeAST* unnest(NodeNamespace& node) {
		m_prefixes = "";
		return node.accept(*this);
	}

private:
	NodeAST* visit(NodeNamespace& node) override {
		add_prefix(node.prefix);
		node.prefix = m_prefixes;
		node.members->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeStatement &node) override {
		if (auto ns = node.statement->cast<NodeNamespace>()) {
			node.statement->accept(*this);
			m_program->namespaces.push_back(unique_ptr_cast<NodeNamespace>(std::move(node.statement)));
			node.statement = std::make_unique<NodeDeadCode>(node.tok);
		}
		return &node;
	}

	NodeAST* visit(NodeBlock &node) override {
		return ASTVisitor::visit(node);
	}

};