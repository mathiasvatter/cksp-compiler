//
// Created by Mathias Vatter on 07.05.24.
//

#pragma once

#include "ASTVisitor.h"

/** @brief Class for AST Optimizations
 * Removing of unused variables, arrays, etc.
 * Constant Folding
 */
class ASTOptimizations : public ASTVisitor {

	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;
	/// remove unused variables
	void visit(NodeSingleDeclareStatement& node) override;

};
