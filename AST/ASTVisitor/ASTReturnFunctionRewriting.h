//
// Created by Mathias Vatter on 31.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"
#include "../../Lowering/ASTLowering.h"

class ASTReturnFunctionRewriting: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
public:
	inline explicit ASTReturnFunctionRewriting(DefinitionProvider *definition_provider): m_def_provider(definition_provider) {}

	// TODO: Call Function Rewriting after Lowering -> call Function Call Hoisting first and then do return statement rewriting as parameters


};