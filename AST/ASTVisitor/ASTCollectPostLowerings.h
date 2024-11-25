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
	explicit ASTCollectPostLowerings(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST * visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);

		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
//		for(auto & func_def : node.function_definitions) {
//			if(!func_def->visited) func_def->accept(*this);
//		}

		node.reset_function_visited_flag();
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
		if(node.bind_definition(m_program)) {
			if(!node.get_definition()->visited) node.get_definition()->accept(*this);
			node.get_definition()->visited = true;
		}
		return &node;
	}

	NodeAST * visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);
		return node.post_lower(m_program);
	}

//	NodeAST * visit(NodeArray& node) override {
//		if(node.size) node.size->accept(*this);
//		if(node.num_elements) node.num_elements->accept(*this);
//		return &node;
//	}

	NodeAST * visit(NodeNumElements& node) override {
		node.array->accept(*this);
		if(node.dimension) node.dimension->accept(*this);
		return node.post_lower(m_program)->accept(*this);
	}

private:
	DefinitionProvider* m_def_provider;
};


