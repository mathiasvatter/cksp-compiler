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
	ASTRegisterReuse register_reuse(m_program);
	for (auto & def : node.function_definitions) {
		def->accept(register_reuse);
	}
	// rename local variables in function definitions
	register_reuse.rename_local_vars();
	ASTParameterPromotion param_promotion(m_program);
	node.accept(param_promotion);

	// second pass to analyze dynamic extend within callbacks and replace with passive_vars
	node.accept(register_reuse);
	node.reset_function_visited_flag();
	return &node;
}


