//
// Created by Mathias Vatter on 15.05.24.
//

#include "ASTGlobalScope.h"
#include "ASTRegisterReuse.h"
#include "ASTParameterPromotion.h"
#include "NormalizeArrayAssign.h"

NodeAST * ASTGlobalScope::visit(NodeProgram &node) {
	m_program = &node;

	NormalizeArrayAssign desugar_single_assign(m_program);
	node.accept(desugar_single_assign);

	// first pass to analyze dynamic extend within function definitions and replace with passive_vars
	// then do parameter promotion directly to global or successively
	static ASTParameterPromotion param_promotion(m_program);
	param_promotion.do_param_promotion(node);

	// second pass to analyze dynamic extend within callbacks and replace with passive_vars
	static ASTRegisterReuse register_reuse(m_program);
	register_reuse.do_register_reuse(node);

	return &node;
}


