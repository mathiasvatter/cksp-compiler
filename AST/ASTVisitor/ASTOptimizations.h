//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../Optimization/ConstantFolding.h"
#include "../../Optimization/ConstantPropagation.h"

/** @brief Class for AST Optimizations
 * Removing of unused variables, arrays, etc.
 * Constant Folding
 */
class ASTOptimizations : public ASTVisitor {
private:

public:
    ASTOptimizations() = default;

	inline NodeAST* visit(NodeProgram& node) override {
		static ConstantPropagation constant_propagation;
		node.accept(constant_propagation);

		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
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

	/// remove unused variables
	inline NodeAST* visit(NodeSingleDeclaration& node) override {
		// remove unused variables -> do not remove UIControls
		if(!node.variable->is_used and node.variable->get_node_type() != NodeType::UIControl) {
			return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
		}

		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);
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
