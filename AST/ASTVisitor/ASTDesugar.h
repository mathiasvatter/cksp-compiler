//
// Created by Mathias Vatter on 21.01.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../Desugaring/ASTDesugaring.h"

/**
 * @brief Desugar into simpler constructs
 *
 * This visitor desugars the following statements:
 * - NodeDeclaration: desugar into single declare statements
 * - NodeAssignment: desugar into single assign statements
 * - NodeForEach: desugar for each loops to for loops
 * - NodeFor: alter for loops to while loops
 * Additionally, it desugars NodeFamily into single declare statements.
 */
class ASTDesugar: public ASTVisitor {
    NodeAST * visit(NodeProgram& node) override;
	NodeAST * visit(NodeFunctionDefinition& node) override;
    /// desugar into single declare statements
	NodeAST * visit(NodeDeclaration& node) override;
    NodeAST * visit(NodeSingleDeclaration& node) override;
    /// desugar into single assign statements
	NodeAST * visit(NodeAssignment& node) override;

    /// desugar for each loops to for loops
	NodeAST * visit(NodeForEach& node) override;
    /// alter for loops to while loops
	NodeAST * visit(NodeFor& node) override;

    NodeAST * visit(NodeBlock& node) override;

	/// desugar into single declare statements
	NodeAST * visit(NodeFamily& node) override;
	/// desugar const block to single declare statements
	NodeAST * visit(NodeConst& node) override;

	/// desugar nested ParamLists [[1,2,3,4]]
	NodeAST * visit(NodeParamList& node) override;

	/// add namespaces
	NodeAST * visit(NodeStruct& node) override;

private:

    std::unique_ptr<NodeBlock> m_global_variable_declarations = std::make_unique<NodeBlock>(Token());
    /// declare necessary compiler variables for iterating etc.
    std::unique_ptr<NodeBlock> declare_compiler_variables();
};

