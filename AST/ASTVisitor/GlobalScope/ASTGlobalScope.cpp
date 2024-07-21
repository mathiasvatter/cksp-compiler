//
// Created by Mathias Vatter on 15.05.24.
//

#include "ASTGlobalScope.h"
#include "ASTRegisterReuse.h"
#include "ASTParameterPromotion.h"
#include "../ASTPrinter.h"
#include "NormalizeSingleDeclareAssign.h"

void ASTGlobalScope::visit(NodeProgram &node) {

	m_program = &node;

	NormalizeSingleDeclareAssign desugar_single_assign(m_program);
	node.accept(desugar_single_assign);

	// first pass to analyze dynamic extend within function definitions and replace with passive_vars
	ASTRegisterReuse regsiter_reuse(m_def_provider, m_program);
	for (auto & def : node.function_definitions) {
		def->accept(regsiter_reuse);
	}
	// rename local variables in function definitions
	regsiter_reuse.rename_local_vars();
	node.debug_print();

	ASTParameterPromotion param_promotion(m_def_provider);
	node.accept(param_promotion);

	// second pass to analyze dynamic extend within callbacks and replace with passive_vars
	node.accept(regsiter_reuse);

}


