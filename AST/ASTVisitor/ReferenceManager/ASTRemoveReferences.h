//
// Created by Mathias Vatter on 13.11.24.
//

#pragma once

#include "../ASTVisitor.h"

/// Works only when declarations of every reference are already set
/// Removes references and their connected data structures and data structures from the reference manager class (map)
class ASTRemoveReferences : public ASTVisitor {
private:
	DefinitionProvider* m_def_provider;
	ReferenceManager* m_ref_manager;
	static void check_for_valid_declaration(const NodeReference& ref) {
		if(!ref.get_declaration()) {
			auto error = CompileError(ErrorType::InternalError, "", "", ref.tok);
			error.m_message = "Declaration was not set.";
			error.m_got = ref.name;
			error.exit();
		}
	}
public:
	explicit ASTRemoveReferences(NodeProgram* main) : m_def_provider(main->def_provider), m_ref_manager(main->ref_manager) {
		m_program = main;
	}

	inline NodeAST* visit(NodeProgram& node) override {
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

	// set visited flag to true
	inline NodeAST *visit(NodeFunctionDefinition &node) override {
		node.visited = true;
		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);

		return &node;
	}

	inline NodeAST *visit(NodeArray &node) override {
		if(node.size) node.size->accept(*this);
		m_ref_manager->remove_data_structure(node.get_shared());
		return &node;
	}
	inline NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
//		check_for_valid_declaration(node);
		m_ref_manager->remove_reference(node.get_declaration(), &node);
		return &node;
	}
	inline NodeAST *visit(NodeVariable &node) override {
		m_ref_manager->remove_data_structure(node.get_shared());
		return &node;
	}
	inline NodeAST *visit(NodeVariableRef &node) override {
//		check_for_valid_declaration(node);
		m_ref_manager->remove_reference(node.get_declaration(), &node);
		return &node;
	}
	inline NodeAST *visit(NodeFunctionHeader &node) override {
		for(auto &param : node.params) param->variable->accept(*this);
		m_ref_manager->remove_data_structure(node.get_shared());
		return &node;
	}
	inline NodeAST *visit(NodeFunctionHeaderRef &node) override {
		if(node.args) node.args->accept(*this);
//		check_for_valid_declaration(node);
		m_ref_manager->remove_reference(node.get_declaration(), &node);
		return &node;
	}
	inline NodeAST *visit(NodeNDArray &node) override {
		if(node.sizes) node.sizes->accept(*this);
		m_ref_manager->remove_data_structure(node.get_shared());
		return &node;
	}
	inline NodeAST *visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);
//		check_for_valid_declaration(node);
		m_ref_manager->remove_reference(node.get_declaration(), &node);
		return &node;
	}
	inline NodeAST *visit(NodePointer &node) override {
		m_ref_manager->remove_data_structure(node.get_shared());
		return &node;
	}
	inline NodeAST *visit(NodePointerRef &node) override {
//		check_for_valid_declaration(node);
		m_ref_manager->remove_reference(node.get_declaration(), &node);
		return &node;
	}
	inline NodeAST *visit(NodeList &node) override {
		for(auto & b : node.body) b->accept(*this);
		m_ref_manager->remove_data_structure(node.get_shared());
		return &node;
	}
	inline NodeAST *visit(NodeListRef &node) override {
		node.indexes->accept(*this);
//		check_for_valid_declaration(node);
		m_ref_manager->remove_reference(node.get_declaration(), &node);
		return &node;
	}
	inline NodeAST *visit(NodeAccessChain &node) override {
		for(auto & c : node.chain) c->accept(*this);

		return &node;
	}


};