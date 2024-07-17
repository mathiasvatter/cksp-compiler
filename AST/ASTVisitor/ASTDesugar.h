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
    void visit(NodeProgram& node) override;
	void visit(NodeFunctionDefinition& node) override;
    /// desugar into single declare statements
    void visit(NodeDeclaration& node) override;
    void visit(NodeSingleDeclaration& node) override;
    /// desugar into single assign statements
    void visit(NodeAssignment& node) override;

    /// desugar for each loops to for loops
	void visit(NodeForEach& node) override;
    /// alter for loops to while loops
    void visit(NodeFor& node) override;

    void visit(NodeBlock& node) override;

	/// desugar into single declare statements
	void visit(NodeFamily& node) override;
	/// desugar const block to single declare statements
	void visit(NodeConst& node) override;

	/// desugar nested ParamLists [[1,2,3,4]]
	void visit(NodeParamList& node) override;

	/// add namespaces
	void visit(NodeStruct& node) override;

private:

    std::unique_ptr<NodeBlock> m_global_variable_declarations = std::make_unique<NodeBlock>(Token());
    /// declare necessary compiler variables for iterating etc.
    std::unique_ptr<NodeBlock> declare_compiler_variables();
};

