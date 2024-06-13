//
// Created by Mathias Vatter on 15.05.24.
//

#include "ASTGlobalScope.h"
#include "ASTRegisterReuse.h"
#include "ASTParameterPromotion.h"
#include "../ASTPrinter.h"
#include "../../../Desugaring/DesugarSingleAssign.h"

void ASTGlobalScope::visit(NodeProgram &node) {
	ASTPrinter printer;

	m_program = &node;

	DesugarSingleAssign desugar_single_assign(m_program);
	node.accept(desugar_single_assign);

	// first pass to analyze dynamic extend within function definitions and replace with passive_vars
	ASTRegisterReuse dyn_extend(m_def_provider, m_program);
	for (auto & def : node.function_definitions) {
		def->accept(dyn_extend);
	}
	// rename local variables in function definitions
	dyn_extend.rename_local_vars();

	ASTParameterPromotion lambda_lifting(m_def_provider);
	node.accept(lambda_lifting);

	// second pass to analyze dynamic extend within callbacks and replace with passive_vars
	node.accept(dyn_extend);

	m_program->init_callback->statements->prepend_body(std::move(m_program->global_declarations));
	m_program->global_declarations = std::make_unique<NodeBlock>(node.tok);
	m_program->global_declarations->parent = m_program;

	node.accept(printer);
	printer.generate("/Users/Mathias/Scripting/ksp-compiler/printed.txt");
}


