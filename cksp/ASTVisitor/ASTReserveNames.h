//
// Created by Mathias Vatter on 29.07.26.
//

#pragma once

#include "ASTVisitor.h"

/**
 * Reserves all names that were present in the parsed source before desugaring
 * starts creating compiler-owned variables.
 *
 * This is deliberately separate from ASTVariableChecking: desugaring already
 * needs fresh names, while variable checking registers declarations only after
 * desugaring. In particular, recompiling generated KSP would otherwise create
 * another `_iter0` before the parsed `_iter0` declaration became visible.
 */
class ASTReserveNames final : public ASTVisitor {
	DefinitionProvider* m_def_provider;

	void reserve(const NodeDataStructure& node) const {
		if (!node.name.empty()) {
			m_def_provider->reserve_name(node.name);
		}
	}

public:
	explicit ASTReserveNames(NodeProgram* program)
		: m_def_provider(program->def_provider) {
		m_program = program;
		m_def_provider->reset_generated_names();
	}

	NodeAST* visit(NodeVariable& node) override {
		reserve(node);
		return &node;
	}

	NodeAST* visit(NodePointer& node) override {
		reserve(node);
		return &node;
	}

	NodeAST* visit(NodeArray& node) override {
		reserve(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeNDArray& node) override {
		reserve(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionHeader& node) override {
		reserve(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeUIControl& node) override {
		reserve(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeList& node) override {
		reserve(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeConst& node) override {
		reserve(node);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeStruct& node) override {
		reserve(node);
		return ASTVisitor::visit(node);
	}
};
