//
// Created by Mathias Vatter on 30.07.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTDataStructureLowering: public ASTVisitor {
private:
	DefinitionProvider* m_def_provider;
public:
	explicit ASTDataStructureLowering(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	inline NodeAST* visit(NodeProgram& node) override {
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
		node.merge_function_definitions();
		node.reset_function_visited_flag();
		return &node;
	};

	inline NodeAST* visit(NodeFor& node) override {
		node.body->accept(*this);
		return node.desugar(m_program)->accept(*this);
	}

	inline NodeAST* visit(NodeSingleDeclaration &node) override {
		node.variable->accept(*this);
		if(node.value) node.value ->accept(*this);
		return &node;
	}

	inline NodeAST* visit(NodeSingleAssignment &node) override {
		node.l_value->accept(*this);
		node.r_value ->accept(*this);
		return &node;
	}

	inline NodeAST * visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		return node.data_lower(m_program);
	}

	inline NodeAST * visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		return node.data_lower(m_program);
	}

	inline NodeAST * visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);
		return node.data_lower(m_program);
	}

	inline NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		return &node;
	};

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			node.get_definition()->accept(*this);
			node.get_definition()->visited = true;
		}
		return &node;
	};
};