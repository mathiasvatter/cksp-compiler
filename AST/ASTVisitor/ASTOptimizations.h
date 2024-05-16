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
	ConstantFolding constant_folding;
    std::unordered_map<NodeAST *, std::unique_ptr<NodeStatement>> m_function_inlines;
public:
    explicit ASTOptimizations(std::unordered_map<NodeAST*, std::unique_ptr<NodeStatement>> m_function_inlines);
    /// Function inlining checking
    void visit(NodeStatement& node) override;
	/// do constant folding for int and reals
	void visit(NodeBinaryExpr& node) override;
	/// remove unused variables
	void visit(NodeSingleDeclareStatement& node) override;

};
