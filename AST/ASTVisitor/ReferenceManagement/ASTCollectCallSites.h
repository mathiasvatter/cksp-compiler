//
// Created by Mathias Vatter on 13.11.24.
//

#pragma once

#include "../ASTVisitor.h"

/// Works only when declarations of every reference are already set
/// Removes references and their connected data structures and data structures from the reference manager class (map)
class ASTCollectCallSites final : public ASTVisitor {
	DefinitionProvider* m_def_provider;

public:
	explicit ASTCollectCallSites(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* do_collect_call_sites(NodeAST& node) {
		m_program->reset_function_visited_flag();
		node.accept(*this);
		m_program->reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}
		// for(const auto & func_def : node.function_definitions) {
		// 	if(!func_def->visited) func_def->accept(*this);
		// }

		return &node;
	}

	NodeAST *visit(NodeBlock &node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		if (const auto definition = node.get_definition()) {
			if(!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;
			definition->call_sites.insert(&node);

		}

		return &node;
	}



};