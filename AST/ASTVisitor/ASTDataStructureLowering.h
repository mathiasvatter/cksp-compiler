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

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		m_program->global_declarations->accept(*this);
		for(const auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		node.reset_function_visited_flag();
		return &node;
	}

	// NodeAST* visit(NodeFor& node) override {
	// 	node.body->accept(*this);
	// 	return node.lower(m_program)->accept(*this);
	// }

	NodeAST* visit(NodeSingleDeclaration &node) override {
		node.variable->accept(*this);
		if(node.value) node.value ->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment &node) override {
		node.l_value->accept(*this);
		node.r_value ->accept(*this);
		return &node;
	}

	NodeAST * visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		if(node.num_elements) node.num_elements->accept(*this);
		return node.data_lower(m_program);
	}

	NodeAST * visit(NodeNDArray& node) override {
		if(node.sizes) node.sizes->accept(*this);
		if(node.num_elements) node.num_elements->accept(*this);
		return node.data_lower(m_program);
	}

	NodeAST * visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);
		return node.data_lower(m_program);
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		return &node;
	};

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			auto definition = node.get_definition();
			if(!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;
		}
		return &node;
	};
};