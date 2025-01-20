//
// Created by Mathias Vatter on 09.08.24.
//

#pragma once

#include "../ASTVisitor.h"


class ReturnPathValidator: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;

public:
	inline explicit ReturnPathValidator(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	bool inline do_return_path_validation(NodeFunctionDefinition& def) {
		return def.accept(*this);
	}

private:

	NodeAST* visit(NodeFunctionDefinition &node) override {
		node.body->accept(*this);
		return &node;
	}

};