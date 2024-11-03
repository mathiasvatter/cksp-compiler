//
// Created by Mathias Vatter on 20.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"

/**
 * @class ASTCollectPostLowerings
 */
class ASTCollectPostLowerings: public ASTVisitor {
public:
	explicit ASTCollectPostLowerings(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {}

	NodeAST * visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);

		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}

		return &node;
	}

	/// flatten blocks
	NodeAST * visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten(true);
		return &node;
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.get_definition(m_program)) {
			if(!node.definition->visited) node.definition->accept(*this);
			node.definition->visited = true;
		}
		return &node;
	}

	NodeAST * visit(NodeNumElements& node) override {
		return node.post_lower(m_program)->accept(*this);
	}

private:
	DefinitionProvider* m_def_provider;
};


