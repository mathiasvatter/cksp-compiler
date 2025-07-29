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
	std::mutex m_name_mutex;

	// rename reference if declaration is function param
	static void rename_func_param_ref(NodeReference& node) {
		if (const auto decl = node.get_declaration()) {
			if (decl->is_function_param() or is_function_return(decl)) {
				node.name = decl->name;
			}
		}
	}

	static bool is_function_return(const std::shared_ptr<NodeDataStructure>& var) {
		return var->parent and var->parent->cast<NodeFunctionDefinition>();
	}

public:
	explicit UniqueParameterNamesProvider(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	// do renaming by visiting all program functions
	NodeAST* do_renaming(NodeProgram& node) {
		parallel_for_each(node.function_lookup.begin(), node.function_lookup.end(),
			[&](const auto & func_pair) {
				for (auto& func_def : func_pair.second) {
					if (auto func = func_def.lock()) {
						if (func->header->has_no_params() and !func->return_variable) return;
						func->accept(*this);
					}
				}
			}
		);
		// for (auto& func_def : node.function_lookup) {
		// 	for (auto& fun : func_def.second) {
		// 		if (auto func = fun.lock()) {
		// 			if (func->header->has_no_params() and !func->return_variable) continue;
		// 			func->accept(*this);
		// 		}
		// 	}
		// }
		// for(const auto & func_def : node.function_definitions) {
		// 	if (func_def->header->has_no_params() and !func_def->return_variable) continue;
		// 	func_def->accept(*this);
		// }
		return &node;
	}

	NodeAST* do_renaming(NodeFunctionDefinition& node) {
		if (node.header->has_no_params() and !node.return_variable) return &node;
		return node.accept(*this);
	}

private:
	NodeAST* visit(NodeFunctionDefinition& node) override {
		node.header ->accept(*this);
		if (node.return_variable) {
			auto& ret_var = node.return_variable.value();
			ret_var->accept(*this);
			std::string fresh_name;
			{
				std::lock_guard<std::mutex> lock(m_name_mutex);
				fresh_name = m_def_provider->get_fresh_name(ret_var->name);
			}
			ret_var->name = std::move(fresh_name);
		}
		node.body->accept(*this);
		return &node;
	}

	// rename all function parameters
	NodeAST* visit(NodeFunctionParam& node) override {
		node.variable->accept(*this);
		if (node.variable->is_function_param()) { // and node.variable->name != "self") {
			std::string fresh_name;
			{
				std::lock_guard<std::mutex> lock(m_name_mutex);
				fresh_name = m_def_provider->get_fresh_name(node.variable->name);
			}
			node.variable->name = std::move(fresh_name);
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
		if (node.sizes) node.sizes->accept(*this);
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
