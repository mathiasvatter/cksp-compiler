//
// Created by Mathias Vatter on 15.05.24.
//

#pragma once

#include "../ASTVisitor.h"
#include "../../../misc/Gensym.h"


class ASTGlobalScope : public ASTVisitor {
protected:
	DefinitionProvider* m_def_provider;

public:
	explicit ASTGlobalScope(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {}

	void visit(NodeProgram& node) override;





};
