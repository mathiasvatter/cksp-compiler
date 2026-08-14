//
// Created by Mathias Vatter on 10.04.25.
//

#pragma once

#include "ASTVisitor.h"
#include "../CompilerConfig.h"

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
			if (decl->is_function_param() or is_function_return(decl)) {
				const bool is_raw = node.cast<NodeArrayRef>() and node.is_raw_array();
				node.name = is_raw ? "_"+decl->name : decl->name;
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

	/// Renames the formal parameters of every function in the program.
	///
	/// Runs sequentially, in sorted key order, and both halves of that matter. The order in which
	/// functions are visited decides which gensym suffix each parameter gets, so it has to be the
	/// same on every compile: <NodeProgram::function_lookup> is a hash map, and walking it as it
	/// lies ties the generated names to its bucket layout, which shifts as soon as the set of
	/// functions or the bucket count changes. Visiting the keys sorted costs one sort of a few
	/// hundred keys and makes the names reproducible between builds. Sequential is what keeps it
	/// correct on top of that: each visit mutates the shared visitor (*this) and draws from the
	/// one <Gensym>, so the parallel version was both a data race and a source of run-to-run
	/// differences in the compiled output.
	NodeAST* do_renaming(NodeProgram& node) {
		std::vector<const StringIntKey*> keys;
		keys.reserve(node.function_lookup.size());
		for (const auto& entry : node.function_lookup) keys.push_back(&entry.first);
		std::sort(keys.begin(), keys.end(), [](const StringIntKey* a, const StringIntKey* b) {
			if (a->str != b->str) return a->str < b->str;
			return a->num < b->num;
		});
		for (const auto* key : keys) {
			for (auto& func_def : node.function_lookup.at(*key)) {
				if (auto func = func_def.lock()) {
					// skip this definition, not the rest of the overloads sharing its key
					if (func->header->has_no_params() and !func->return_variable) continue;
					func->accept(*this);
				}
			}
		}
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
			ret_var->name = m_def_provider->get_fresh_name(ret_var->name);
		}
		node.body->accept(*this);
		return &node;
	}

	// rename all function parameters
	NodeAST* visit(NodeFunctionParam& node) override {
		node.variable->accept(*this);
		if (node.variable->is_function_param()) { // and node.variable->name != NodeStruct::SELF) {
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
