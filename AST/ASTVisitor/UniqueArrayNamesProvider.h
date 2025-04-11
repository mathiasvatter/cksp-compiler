//
// Created by Mathias Vatter on 10.04.25.
//

#pragma once

#include "ASTVisitor.h"

/**
 * Renames Arrays or NDArrays that are used to describe the size of another array
 * Because in rare cases this can lead to name collisions
 * declare read keyswitch[NUM_KEYSWITCHES,num_elements(keyswitch_idx)] <-
 * in a function with parameter named keyswitch_idx, once the keyswitch array is lowered,
 * num_elements(keyswitch_idx) might be replaced by the parameter of same name
 */
class UniqueArrayNamesProvider final : public ASTVisitor {
	DefinitionProvider* m_def_provider;
	bool m_is_in_size_context = false;
	std::unordered_set<NodeDataStructure*> m_already_renamed;
	std::vector<NodeReference*> m_all_references;

	void rename_if_in_size(const NodeReference& node) {
		if (m_is_in_size_context) {
			if (const auto decl = node.get_declaration()) {
				if (!m_already_renamed.contains(decl.get())) {
					decl->name = m_def_provider->get_fresh_name(decl->name);
					m_already_renamed.insert(decl.get());
				}
			}
		}
	}

public:
	explicit UniqueArrayNamesProvider(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	NodeAST* do_renaming(NodeProgram& node) {
		m_all_references.clear();
		m_already_renamed.clear();
		m_is_in_size_context = false;
		node.global_declarations->accept(*this);
		node.init_callback->accept(*this);
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
		for (const auto& ref : m_all_references) {
			if (const auto decl = ref->get_declaration()) {
				if (m_already_renamed.contains(decl.get())) {
					ref->name = decl->name;
				}
			}
		}
		return &node;
	}

	NodeAST* visit(NodeArray& node) override {
		if (node.size) {
			m_is_in_size_context = true;
			node.size->accept(*this);
			m_is_in_size_context = false;
		}
		return &node;
	}

	NodeAST* visit(NodeNDArray& node) override {
		if (node.sizes) {
			m_is_in_size_context = true;
			node.sizes->accept(*this);
			m_is_in_size_context = false;
		}
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if (node.index) node.index->accept(*this);
		rename_if_in_size(node);
		m_all_references.push_back(&node);
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		rename_if_in_size(node);
		m_all_references.push_back(&node);
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if (node.indexes) node.indexes->accept(*this);
		rename_if_in_size(node);
		m_all_references.push_back(&node);
		return &node;
	}
};
