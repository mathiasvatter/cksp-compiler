//
// Created by Mathias Vatter on 10.06.24.
//

#pragma once

#include "ASTGlobalScope.h"
#include "../../../Desugaring/ASTDesugaring.h"

/**
 * @brief Handles the dynamic extension of variables/arrays within different scopes in functions and callbacks and
 * replaces them with typed variables whose dynamic extension has expired.
 * Declarations are replaced by assignments with neutral elements, or (when promoted by neighbor class ParameterPromotion) removed.
 *
 * - Creates a map of "passive variables" with hash values based on variable types (and array sizes).
 * 	 Once a scope is exited, variables whose dynamic extension has expired are added to the map.
 * - For each new declaration, it checks if a variable with the same type already exists in the map.
 *   If so, the variable is replaced by a reference to the map.
 * - Replaced declarations are substituted by assignments with neutral elements or already assigned values.
 * - Tracks all variables and references in lists and renames them using Gensym to avoid variable capturing when a free
 *   "passive variable" has the same name as a variable in the scope.
 */
class ASTRegisterReuse : public ASTGlobalScope {
public:
	explicit ASTRegisterReuse(DefinitionProvider *definition_provider, NodeProgram* program) : ASTGlobalScope(definition_provider) {
		m_program = program;
	}

	inline NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		// update because of potentially altered param counts after lambda lifting
		node.update_function_lookup();
		m_def_provider->refresh_scopes();
		clear_passive_vars();

		m_program->global_declarations->accept(*this);
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		rename_local_vars();

		// move all passive_vars declarations to global scope
		auto local_declare_statements = std::make_unique<NodeBlock>(node.tok);
		for(auto & callback : m_all_callback_decl) {
			// do not replace with assign statements if in init callback already
			for(auto & local_decl : callback.second) {
				// set local to false to avoid renaming in rename_local_vars
				local_decl->variable->is_local = false;
				if(callback.first == m_program->init_callback) continue;
				local_declare_statements->add_as_stmt(
						std::make_unique<NodeSingleDeclaration>(
							local_decl->variable,
							nullptr, node.tok
						)
				);
				local_decl->replace_with(std::move(to_assign_statement(*local_decl)));
//				auto node_assign_statement = local_decl->to_assign_stmt();
//				local_decl->replace_with(std::move(node_assign_statement));
			}
		}
		node.init_callback->statements->prepend_body(std::move(local_declare_statements));
		return &node;
	}

	inline bool rename_local_vars() {
		// rename local passive_vars with gensym and add to global scope
		for(auto & local_var : m_all_local_vars) {
			// in case it is ::num_elements ->  the suffix needs to be retained
			if(local_var->is_num_elements_constant()) {
				local_var->name = m_gensym.fresh(loc_var_prefix) + OBJ_DELIMITER + "num_elements";
			} else {
				local_var->name = m_gensym.fresh(loc_var_prefix);
			}
			m_def_provider->set_declaration(local_var, false);
		}
		// rename all local references with their new passive_var names
		for(auto & local_ref : m_all_local_references) {
			if(!local_ref->get_declaration()) continue;
			local_ref->get_declaration()->is_used = true;
			if(local_ref->is_local) local_ref->name = local_ref->get_declaration()->name;
		}
		m_all_local_vars.clear();
		m_all_local_references.clear();
		return true;
	}

	inline NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_program->current_callback = nullptr;
		return &node;
	}

	inline NodeAST* visit(NodeBlock &node) override {
		m_current_body = &node;
		if(node.scope) {
			m_def_provider->add_scope();
			m_passive_vars_replace.emplace_back();
		}
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		if(node.scope) {
			auto passive_vars = m_def_provider->remove_scope();
			// only add new passive vars on thread_safe callbacks and functions not using 'wait'
			if(is_thread_safe_env()) {
				// add free memory vars which dynamic extent has ended to passive_vars vector
				add_passive_vars(passive_vars);
			}
			// set back passive_var index since scope has ended
			for(auto & idx : m_passive_vars_idx) {
				idx.second = 0;
			}
			// clear passive_var replace map
			m_passive_vars_replace.pop_back();
		}
		node.flatten();
		return &node;
	}

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);

		// do not visit definition -> because passive var allocation is separate between callbacks and functions
		return &node;
	}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		// start every function definition with a clean slate
		clear_passive_vars();
		m_program->function_call_stack.push(node.weak_from_this());
		m_def_provider->add_scope();
		node.header->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		m_def_provider->remove_scope();
		m_program->function_call_stack.pop();
		return &node;
	}

	inline NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->determine_locality(m_program, m_current_body);


		if(m_current_body != m_program->global_declarations.get()) {
			// constant local declarations shall not be reused. But still need to be renamed to avoid name clashes
			if (node.variable->is_local and node.variable->data_type == DataType::Const) {
				node.variable->is_local = false;
				node.variable->is_global = true;
				if(node.value) node.value->accept(*this);
				auto node_global_const = std::make_unique<NodeSingleDeclaration>(
					node.variable,
					std::move(node.value),
					node.tok
				);
				// set declaration to local to avoid name clashes
				m_def_provider->set_declaration(node_global_const->variable, false);
				// add to vector here for later renaming and to avoid it turning into a passive var
				m_all_local_vars.push_back(node.variable);
				m_program->global_declarations->add_as_stmt(std::move(node_global_const));
				return node.remove_node();
			}
		} else {
			node.variable->is_local = false;
			node.variable->is_global = true;
			if(node.value) node.value->accept(*this);
			m_def_provider->set_declaration(node.variable, !node.variable->is_local);
			return &node;
		}

		if(node.variable->is_local) {
			if(is_thread_safe_env()) {
				if (auto free_passive_var = get_free_passive_var(*node.variable)) {
					m_passive_vars_replace.back().insert({node.variable->name, free_passive_var});

					auto replacement = to_assign_statement(node);
					// visit replacement (assign statement) to replace local var with passive_var
					replacement->accept(*this);
					return node.replace_with(std::move(replacement));
				}
			}
		}
		// only add var to local scope if it is not replaced by passive_var
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);
		// add local vars to lists for later renaming
		if(node.variable->is_local) {
			if(m_program->current_callback) m_all_callback_decl[m_program->current_callback].push_back(&node);
			m_all_local_vars.push_back(node.variable);
		}
		return &node;
	}

	inline NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);

		// add all references in local scope to vector for later passive_var replacement
		m_all_local_references.push_back(&node);

		if(!m_passive_vars_replace.empty()) {
			// search if declaration was local var and has been replaced by passive_var -> replace declaration and reference name
			if(auto new_declaration = get_new_declaration(node.name)) {
				node.match_data_structure(new_declaration);
				node.name = new_declaration->name;
				return &node;
			}
		}

		auto node_declaration = m_def_provider->get_declaration(node);
		if(!node_declaration) DefinitionProvider::throw_declaration_error(node).exit();

		node.match_data_structure(node_declaration);
		return &node;
	}

	inline NodeAST * visit(NodeArray& node) override {
		node.determine_locality(m_program, m_current_body);

		if(node.size) node.size->accept(*this);

		m_def_provider->set_declaration(node.get_shared(), !node.is_local);
		m_gensym.ingest(node.name);
		return &node;
	}

	inline NodeAST * visit(NodeVariableRef& node) override {
		// add all references in local scope to vector for later passive_var replacement
		m_all_local_references.push_back(&node);

		if(!m_passive_vars_replace.empty()) {
			// search if declaration was local var and has been replaced by passive_var -> replace declaration and reference name
			if(auto new_declaration = get_new_declaration(node.name)) {
				node.match_data_structure(new_declaration);
				node.name = new_declaration->name;
				return &node;
			}
		}

		auto node_declaration = m_def_provider->get_declaration(node);
		if(!node_declaration) {
//			if(node.data_type == DataType::Const) {
//				// do not throw error for const variables
//				return &node;
//			}
			DefinitionProvider::throw_declaration_error(node).exit();
		}

		node.match_data_structure(node_declaration);
		return &node;
	}

	inline NodeAST* visit(NodeVariable& node) override {
		node.determine_locality(m_program, m_current_body);
		m_def_provider->set_declaration(node.get_shared(), !node.is_local);
		m_gensym.ingest(node.name);
		return &node;
	}

	bool clear_passive_vars() {
		m_passive_vars_map.clear();
		return true;
	}



private:
	std::string loc_var_prefix = "loc_";
	Gensym m_gensym;
	NodeBlock* m_current_body = nullptr;

	/// track all vars and their replacements
	std::unordered_map<std::shared_ptr<NodeDataStructure>, std::shared_ptr<NodeDataStructure>> m_all_replacements;

	/// vector for all local declarations in callbacks
	std::unordered_map<NodeCallback*, std::vector<NodeSingleDeclaration*>> m_all_callback_decl = {};
	/// vector for all local vars in functions -> do not get moved into on init
	std::vector<std::shared_ptr<NodeDataStructure>> m_all_local_vars = {};
	/// hash values are the types
	std::unordered_map<std::string, std::vector<std::shared_ptr<NodeDataStructure>>> m_passive_vars_map;
	std::unordered_map<std::string, int> m_passive_vars_idx;
	inline void add_passive_vars(const std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>, StringHash, StringEqual>& map2) {
		for(auto & var : map2) {
			if(var.second->data_type == DataType::Mutable)
				m_passive_vars_map[get_passive_var_hash(*var.second)].push_back(var.second);
		}
	};

	/// get next free passive_var for given type
	inline std::shared_ptr<NodeDataStructure> get_free_passive_var(NodeDataStructure& data) {
		auto hash = get_passive_var_hash(data);
		auto &vec = m_passive_vars_map[hash];
		auto &idx = m_passive_vars_idx[hash];
		if(idx < vec.size()) {
			return vec[idx++];
		}
		return nullptr;
	}
	/// search for new declaration to reference if declaration is replaced by passive_var
	inline std::shared_ptr<NodeDataStructure> get_new_declaration(const std::string& ref_name) {
		for (auto rit = m_passive_vars_replace.rbegin(); rit != m_passive_vars_replace.rend(); ++rit) {
			auto it = rit->find(ref_name);
			if (it != rit->end()) {
				return it->second;
			}
		}
		return nullptr;
	}
	/// map for old datastructure name (as keys) that get replaced by new datastructures (passive_vars) (as values)
	std::vector<std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>>> m_passive_vars_replace;
	/// vector for all local references that have been replaced by passive_var references
	std::vector<NodeReference*> m_all_local_references;

};
