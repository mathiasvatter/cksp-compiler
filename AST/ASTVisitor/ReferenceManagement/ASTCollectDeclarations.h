//
// Created by Mathias Vatter on 13.11.24.
//

#pragma once

#include "../ASTVisitor.h"

/// Works only when declarations of every reference are already set
/// Removes references and their connected data structures and data structures from the reference manager class (map)
class ASTCollectDeclarations final : public ASTVisitor {
	DefinitionProvider* m_def_provider;
	std::stack<NodeBlock*> m_current_block;

	NodeBlock* get_current_block() const {
		if (m_current_block.empty()) return nullptr;
		return m_current_block.top();
	}

public:
	explicit ASTCollectDeclarations(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* do_collect_declarations(NodeAST& node) {
		// update function lookup map because of altered param counts after lambda lifting
		m_program->merge_function_definitions();
		m_program->update_function_lookup();
		// erase all previously saved scopes
		m_def_provider->refresh_scopes();
		m_def_provider->refresh_data_vectors();
		return node.accept(*this);
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
		for(const auto & func_def : node.function_definitions) {
			if(!func_def->visited) func_def->accept(*this);
		}

		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST *visit(NodeBlock &node) override {
		m_current_block.push(&node);
		if(node.scope) {
			if(!node.parent->cast<NodeStruct>()) {
				m_def_provider->add_scope();
			}
		}
		// if body is in function definition, copy over last scope of header variables
		if(node.parent->cast<NodeFunctionDefinition>()) {
			m_def_provider->copy_last_scope();
		}
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		if(node.scope) {
			if(!node.parent->cast<NodeStruct>()) {
				m_def_provider->remove_scope();
			}
		}
		m_current_block.pop();
		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition &node) override {
		m_program->function_call_stack.push(node.weak_from_this());
		m_def_provider->add_scope();

		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);

		m_def_provider->remove_scope();
		m_program->function_call_stack.pop();
		return &node;
	}

	NodeAST *visit(NodeSingleDeclaration &node) override {
		node.variable->determine_locality(m_program, get_current_block());
		node.variable->accept(*this);
		m_def_provider->set_declaration(node.variable, !node.variable->is_local);
		if(node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST *visit(NodeFunctionParam &node) override {
		node.variable->determine_locality(m_program, get_current_block());
		node.variable->accept(*this);
		m_def_provider->set_declaration(node.variable, !node.variable->is_local);
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		auto node_declaration = m_def_provider->get_declaration(node);
		if (node_declaration) node.match_data_structure(node_declaration);
		return &node;
	}

	NodeAST *visit(NodeVariableRef &node) override {
		auto node_declaration = m_def_provider->get_declaration(node);
		if (node_declaration) node.match_data_structure(node_declaration);
		return &node;
	}
	NodeAST *visit(NodeFunctionHeader &node) override {
		node.determine_locality(m_program, get_current_block());
		m_def_provider->set_declaration(node.get_shared(), !node.is_local);

		for(auto &param : node.params) param->accept(*this);
		return &node;
	}
	NodeAST *visit(NodeFunctionHeaderRef &node) override {
		if(node.args) node.args->accept(*this);
		auto node_declaration = m_def_provider->get_declaration(node);
		if (node_declaration) node.match_data_structure(node_declaration);
		return &node;
	}

	NodeAST *visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);
		auto node_declaration = m_def_provider->get_declaration(node);
		if (node_declaration) node.match_data_structure(node_declaration);
		return &node;
	}

	NodeAST *visit(NodePointerRef &node) override {
		auto node_declaration = m_def_provider->get_declaration(node);
		if (node_declaration) node.match_data_structure(node_declaration);
		return &node;
	}
	NodeAST *visit(NodeList &node) override {
		node.determine_locality(m_program, get_current_block());
		m_def_provider->set_declaration(node.get_shared(), !node.is_local);
		for(auto & b : node.body) b->accept(*this);
		return &node;
	}
	NodeAST *visit(NodeListRef &node) override {
		node.indexes->accept(*this);
		auto node_declaration = m_def_provider->get_declaration(node);
		if (node_declaration) node.match_data_structure(node_declaration);
		return &node;
	}



};