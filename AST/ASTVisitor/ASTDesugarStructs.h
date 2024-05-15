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
 * - NodeDeclareStatement: desugar into single declare statements
 * - NodeAssignStatement: desugar into single assign statements
 * - NodeForEachStatement: desugar for each loops to for loops
 * - NodeForStatement: alter for loops to while loops
 * Additionally, it desugars NodeFamilyStatement into single declare statements.
 */
class ASTDesugarStructs: public ASTVisitor {
    void visit(NodeProgram& node) override;

    /// desugar into single declare statements
    void visit(NodeDeclareStatement& node) override;
    /// desugar into single assign statements
    void visit(NodeAssignStatement& node) override;

    /// desugar for each loops to for loops
	void visit(NodeForEachStatement& node) override;
    /// alter for loops to while loops
    void visit(NodeForStatement& node) override;

    void visit(NodeBody& node) override;

	/// desugar into single declare statements
	void visit(NodeFamilyStatement& node) override;

	/// desugar nested ParamLists [[1,2,3,4]]
	void visit(NodeParamList& node) override;

private:
    NodeProgram* m_program = nullptr;

};

