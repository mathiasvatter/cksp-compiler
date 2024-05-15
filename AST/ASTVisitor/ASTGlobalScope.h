//
// Created by Mathias Vatter on 15.05.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTGlobalScope : public ASTVisitor {
public:
	explicit ASTGlobalScope(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {}

//	void inline visit(NodeProgram& node) override;

private:
	DefinitionProvider* m_def_provider = nullptr;

	std::stack<std::unique_ptr<NodeDataStructure>> m_passive_vars;
};
