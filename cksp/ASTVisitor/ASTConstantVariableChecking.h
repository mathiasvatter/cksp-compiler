//
// Created by Mathias Vatter on 30.07.26.
//

#pragma once

#include "ASTVisitor.h"

class ASTConstantVariableChecking final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	std::stack<NodeBlock*> m_current_block;

	[[nodiscard]] NodeBlock* get_current_block() const {
		if (m_current_block.empty()) return nullptr;
		return m_current_block.top();
	}


public:
	explicit ASTConstantVariableChecking(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}


	NodeAST* visit(NodeProgram& node) override {
		m_def_provider->refresh_scopes();
		m_def_provider->refresh_declaration_candidates();
		m_def_provider->refresh_data_vectors();

		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}
		node.reset_function_visited_flag();
		m_def_provider->refresh_scopes();
		return &node;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;
		if (node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		m_program->current_callback = nullptr;
		return &node;
	}


	NodeAST* visit(NodeBlock &node) override {
		node.flatten();
		m_current_block.push(&node);
		node.determine_scope();
		if(node.scope) m_def_provider->add_scope();
		// if body is in function definition, copy over last scope of header variables
		if(node.parent->cast<NodeFunctionDefinition>() or node.parent->cast<NodeForEach>()) {
			m_def_provider->copy_last_scope();
		}
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		if(node.scope) m_def_provider->remove_scope();
		m_current_block.pop();
		return &node;
	}

	// needs to be here, otherwise it gets seen as local declaration inside a block
	NodeAST* visit(NodeConst& node) override {
		for(auto& stmt : node.constants->statements) {
			stmt->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeStruct& node) override {
		// m_current_struct = &node;
		m_def_provider->add_scope();
		// add extra members scope
		m_def_provider->add_scope();
		node.members->accept(*this);
		for(const auto & m : node.methods) {
			m->accept(*this);
		}
		// remove the members scope
		m_def_provider->remove_scope();
		m_def_provider->remove_scope();
		// m_current_struct = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition &node) override {
		node.visited = true;

		m_program->function_definition_stack.push(node.weak_from_this());
		m_def_provider->add_scope();

		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);

		m_def_provider->remove_scope();
		m_program->function_definition_stack.pop();
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (!node.variable->cast<NodeUIControl>() and node.variable->data_type == DataType::Const) {
			node.variable->determine_locality(m_program, get_current_block());
			m_def_provider->set_declaration(node.variable->get_shared(), !node.variable->is_local);
		}
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if(node.get_declaration()) return &node;
		const auto node_declaration = m_def_provider->get_declaration(node);
		if (node_declaration) {
			node.match_data_structure(node_declaration);
		}
		return &node;
	}

};
