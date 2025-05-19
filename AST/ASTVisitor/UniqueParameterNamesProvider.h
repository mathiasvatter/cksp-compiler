//
// Created by Mathias Vatter on 10.04.25.
//

#pragma once

#include "ASTVisitor.h"

/**
 * Renames function formal parameters to prevent name collisions with sizes of ndarrays
 * Because in rare cases this can lead to name collisions
 * declare read keyswitch[NUM_KEYSWITCHES,num_elements(keyswitch_idx)] <-
 * in a function with parameter named keyswitch_idx, once the keyswitch array is lowered,
 * num_elements(keyswitch_idx) might be replaced by the parameter of same name
 */
class UniqueParameterNamesProvider final : public ASTVisitor {
	DefinitionProvider* m_def_provider;

	// rename reference if declaration is function param
	static void rename_func_param_ref(NodeReference& node) {
		if (const auto decl = node.get_declaration()) {
			if (decl->is_function_param()) {
				node.name = decl->name;
			}
		}
	}

public:
	explicit UniqueParameterNamesProvider(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	// do renaming by visiting all program functions
	NodeAST* do_renaming(NodeProgram& node) {
		for(const auto & func_def : node.function_definitions) {
			if (func_def->header->has_no_params()) continue;
			func_def->accept(*this);
		}
		return &node;
	}

	NodeAST* do_renaming(NodeFunctionDefinition& node) {
		if (node.header->has_no_params()) return &node;
		return node.accept(*this);
	}

private:
	// rename all function parameters
	NodeAST* visit(NodeFunctionParam& node) override {
		node.variable->accept(*this);
		if (node.variable->is_function_param()) {
			node.variable->name = m_def_provider->get_fresh_name(node.variable->name);
		}
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if (node.index) node.index->accept(*this);
		rename_func_param_ref(node);
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		rename_func_param_ref(node);
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if (node.indexes) node.indexes->accept(*this);
		rename_func_param_ref(node);
		return &node;
	}

	NodeAST* visit(NodePointerRef& node) override {
		rename_func_param_ref(node);
		return &node;
	}

	NodeAST* visit(NodeIf& node) override {
		return ASTVisitor::visit(node);
	}
};
