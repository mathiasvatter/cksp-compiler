//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../Optimization/ConstantFolding.h"
#include "../../Optimization/ConstantPropagation.h"
#include "../../Optimization/VariablePruning.h"

/** @brief Class for AST Optimizations
 * Constant Propagation
 * Variable Pruning
 * Constant Folding
 */
class ASTOptimizations : public ASTVisitor {
private:

public:
    ASTOptimizations() = default;

	inline NodeAST* visit(NodeProgram& node) override {
		static ConstantPropagation constant_propagation;
		node.accept(constant_propagation);
		static VariablePruning variable_pruning;
		node.accept(variable_pruning);

		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & func_def : node.function_definitions) {
			func_def->accept(*this);
		}
		node.merge_function_definitions();
		node.reset_function_visited_flag();
		return &node;
	}

	/// do constant folding for int and reals
	inline NodeAST* visit(NodeBinaryExpr& node) override {
		static ConstantFolding constant_folding;
		node.accept(constant_folding);
		return &node;
	}

    /// do node body cleanup
	inline NodeAST* visit(NodeBlock& node) override {
		for(auto& statement : node.statements) {
			statement->accept(*this);
		}
		node.flatten();
		return &node;
	}
};
