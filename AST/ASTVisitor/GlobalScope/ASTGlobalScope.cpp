//
// Created by Mathias Vatter on 15.05.24.
//

#include "ASTGlobalScope.h"
#include "ASTRegisterReuse.h"
#include "ASTParameterPromotion.h"
#include "../ASTPrinter.h"
#include "NormalizeArrayAssign.h"

NodeAST * ASTGlobalScope::visit(NodeProgram &node) {

	m_program = &node;

	NormalizeArrayAssign desugar_single_assign(m_program);
	node.accept(desugar_single_assign);

	// first pass to analyze dynamic extend within function definitions and replace with passive_vars
	ASTRegisterReuse register_reuse(m_def_provider, m_program);
	for (auto & def : node.function_definitions) {
		def->accept(register_reuse);
	}
	// rename local variables in function definitions
	register_reuse.rename_local_vars();
	register_reuse.clear_all_maps();
	ASTParameterPromotion param_promotion(m_def_provider);
	node.accept(param_promotion);
//	node.debug_print();
	// second pass to analyze dynamic extend within callbacks and replace with passive_vars
	node.accept(register_reuse);
	register_reuse.clear_all_maps();
	node.reset_function_visited_flag();
	return &node;
}


