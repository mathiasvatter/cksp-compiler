//
// Created by Mathias Vatter on 17.08.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

class ASTRelinkGlobalScope final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
public:
	explicit ASTRelinkGlobalScope(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();

		// erase all previously saved scopes
		m_def_provider->refresh_scopes();

		m_def_provider->refresh_data_vectors();

		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}
		node.reset_function_visited_flag();

		relink_global_scope();

		return &node;
	}

	void relink_global_scope() const {
		for(auto & data_struct : m_def_provider->get_all_data_structures()) {
			m_def_provider->set_declaration(data_struct.lock(), true);
		}
		for(auto & reference : m_def_provider->get_all_references()) {
			auto new_declaration = m_def_provider->get_declaration(*reference);
			if(!new_declaration) DefinitionProvider::throw_declaration_error(*reference).exit();
			reference->match_data_structure(new_declaration);
		}
	}

	NodeAST* visit(NodeVariable& node) override {
		m_def_provider->add_to_data_structures(node.get_shared());
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		m_def_provider->add_to_references(&node);
		return &node;
	}

	NodeAST* visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		m_def_provider->add_to_data_structures(node.get_shared());
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		m_def_provider->add_to_references(&node);
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			if(node.kind != NodeFunctionCall::Kind::UserDefined) return &node;
			if(!node.get_definition()->visited) node.get_definition()->body->accept(*this);
			node.get_definition()->visited = true;
		}
		return &node;
	}

};