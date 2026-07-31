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
	ASTVariableChecking& m_checker;

public:
	ASTUIControlLowering(NodeProgram* program, const ConstantDatabase& constant_database, ASTVariableChecking& check)
		: m_lowering(program, &constant_database), m_checker(check) {
		m_program = program;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		// for(const auto & callback : node.callbacks) {
		// 	if(callback.get() != m_program->init_callback) callback->accept(*this);
		// }
		// for(const auto & func_def : node.function_definitions) {
		// 	if(!func_def->visited) func_def->accept(*this);
		// }
		node.reset_function_visited_flag();
		return &node;
	}

	// NodeAST* visit(NodeCallback& node) override {
	// 	if (m_program->init_callback == &node) {
	// 		node.statements->accept(*this);
	// 		return &node;
	// 	}
	// 	// add possibly unknown ui control array controls to their new declarations
	// 	if (node.callback_id) {
	// 		node.callback_id->accept(m_checker);
	// 		node.callback_id->collect_references();
	// 	}
	//
	// }

	NodeAST* visit(NodeBlock& node) override {
		for (const auto& stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (!node.variable->cast<NodeUIControl>()) return &node;
		auto lowered_node = node.accept(m_lowering);
		// lowered_node->accept(m_checker);
		// lowered_node->collect_references();
		return lowered_node;
	}
};
