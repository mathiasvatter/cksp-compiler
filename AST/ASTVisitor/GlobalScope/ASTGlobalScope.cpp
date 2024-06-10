//
// Created by Mathias Vatter on 15.05.24.
//

#include "ASTGlobalScope.h"
#include "ASTDynamicExtend.h"
#include "ASTLambdaLifting.h"

void ASTGlobalScope::visit(NodeProgram &node) {
	m_program = &node;

	// first pass to analyze dynamic extend within function definitions and replace with passive_vars
	ASTDynamicExtend dyn_extend(m_def_provider, m_program);
	for (auto & def : node.function_definitions) {
		def->accept(dyn_extend);
	}
	// rename local variables in function definitions
	dyn_extend.rename_local_vars();

	ASTLambdaLifting lambda_lifting(m_def_provider);
	node.accept(lambda_lifting);

	// second pass to analyze dynamic extend within callbacks and replace with passive_vars
	node.accept(dyn_extend);
}
