//
// Created by Mathias Vatter on 20.08.24.
//

#include "ASTOptimizations.h"

#include "../../Optimization/ConstantFolding.h"
#include "../../Optimization/ConstantPropagation.h"
#include "../../Optimization/VariablePruning.h"
#include "../../Optimization/ConstExprPropagation.h"
#include "../../Optimization/DeadCodeElimination.h"

bool ASTOptimizations::optimize(NodeProgram &node, int iterations) {
	for(int i = 0; i<iterations; i++) {
		static ConstantPropagation constant_propagation;
		node.accept(constant_propagation);
		static ConstExprPropagation const_expr_propagation;
		node.accept(const_expr_propagation);
		static ConstantFolding constant_folding;
		node.accept(constant_folding);
	}
	static VariablePruning variable_pruning;
	node.accept(variable_pruning);
	static DeadCodeElimination dead_code_elimination;
	node.accept(dead_code_elimination);

	return true;
}
