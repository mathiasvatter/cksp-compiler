//
// Created by Mathias Vatter on 30.07.26.
//

#pragma once

#include "ASTVisitor.h"
#include "../Desugaring/DesugarUIControlArray.h"
#include "../Optimization/ConstantDatabase.h"

/**
 * Lowers UI control arrays after constants have been collected, but before
 * variable checking. Generated backing arrays, individual controls and
 * initialization statements therefore participate in the normal frontend
 * validation passes.
 */
class ASTUIControlLowering final : public ASTVisitor {
	DesugarUIControlArray m_lowering;

public:
	ASTUIControlLowering(NodeProgram* program, const ConstantDatabase& constant_database)
		: m_lowering(program, &constant_database) {
		m_program = program;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		// no callback visiting necessary
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		for (const auto& stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (!node.variable->cast<NodeUIControl>()) return &node;
		return node.accept(m_lowering);
	}
};
