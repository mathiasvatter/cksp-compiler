//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../Optimization/ConstantFolding.h"

/** @brief Class for AST Optimizations
 * Removing of unused variables, arrays, etc.
 * Constant Folding
 */
class ASTOptimizations : public ASTVisitor {
private:

public:
    ASTOptimizations() = default;
	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;
	/// remove unused variables
	void visit(NodeSingleDeclaration& node) override;
    /// do node body cleanup
    void visit(NodeBody& node) override;
};
