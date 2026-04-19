//
// Created by Mathias Vatter on 20.08.24.
//

#include "ASTOptimizations.h"

#include "../../Optimization/ConstantFolding.h"
#include "../../Optimization/ConstantPropagation.h"
#include "../../Optimization/VariablePruning.h"
#include "../../Optimization/ConstExprPropagation.h"
#include "../../Optimization/DeadCodeElimination.h"
#include "../../Optimization/ConstantPromotion.h"
#include "../../Optimization/ScalarVarToArray.h"

bool ASTOptimizations::optimize(NodeProgram &node, const OptimizationLevel optimize) {
	int iterations = 0;
	bool do_const_expr_propagation = true;
	if (optimize == OptimizationLevel::None) {
		return false;
	} else if (optimize == OptimizationLevel::Simple) {
		do_const_expr_propagation = false;
		iterations = 1;
	} else if (optimize == OptimizationLevel::Standard) {
		iterations = 1;
	} else if (optimize == OptimizationLevel::Aggressive) {
		iterations = 3;
	}


//	static ConstantPromotion constant_promotion;
//	node.accept(constant_promotion);
	for(int i = 0; i<iterations; i++) {
		static ConstantPropagation constant_propagation;
		node.accept(constant_propagation);
		node.debug_print();
		if(do_const_expr_propagation) {
			static ConstExprPropagation const_expr_propagation;
			node.accept(const_expr_propagation);
			node.debug_print();
		}
		static ConstantFolding constant_folding;
		node.accept(constant_folding);
		node.debug_print();
	}
	static VariablePruning variable_pruning;
	node.accept(variable_pruning);
	node.debug_print();
	static DeadCodeElimination dead_code_elimination;
	node.accept(dead_code_elimination);
	node.debug_print();
	if (optimize == OptimizationLevel::Aggressive) {
		static ScalarVarToArray scalar_var_to_array;
		node.accept(scalar_var_to_array);
		node.debug_print();
	}

	return true;
}
