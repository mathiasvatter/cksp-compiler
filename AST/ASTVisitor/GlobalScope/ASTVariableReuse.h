//
// Created by Mathias Vatter on 10.06.24.
//

#pragma once

#include "../ASTVisitor.h"

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
class ASTVariableReuse final : public ASTVisitor {
public:
	explicit ASTVariableReuse(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
		m_def_provider->refresh_scopes();
		clear_all_maps();
	}

	std::vector<std::shared_ptr<NodeDataStructure>> do_variable_reuse(NodeFunctionDefinition& def) {
		m_program->current_callback = nullptr;
		m_def_provider->refresh_scopes();
		def.accept(*this);
		rename_local_vars();
		auto local_vars = std::move(m_all_local_vars);
		clear_all_maps();
		return std::move(local_vars);
	}

	void do_variable_reuse(NodeProgram& program) {
		m_program = &program;
		m_program->current_callback = nullptr;
		m_def_provider->refresh_scopes();
		program.accept(*this);
		rename_local_vars();
		promote_to_global_vars();
		clear_all_maps();
	}

	bool promote_to_global_vars() const {
		// move all passive_vars declarations to global scope
		auto local_declare_statements = std::make_unique<NodeBlock>(Token());
		for(auto & callback : m_all_callback_decl) {
			// do not replace with assign statements if in init callback already
			for(auto & local_decl : callback.second) {
				// set local to false to avoid renaming in rename_local_vars
				local_decl->variable->is_local = false;
				if(callback.first == m_program->init_callback) continue;
				local_declare_statements->add_as_stmt(
					std::make_unique<NodeSingleDeclaration>(
						local_decl->variable,
						nullptr, local_decl->tok
					)
				);
				local_decl->replace_with(std::move(to_assign_statement(*local_decl)));
			}
		}
		m_program->init_callback->statements->prepend_body(std::move(local_declare_statements));
		return true;
	}

	bool rename_local_vars() {
		// rename local passive_vars with gensym and add to global scope
		for(auto & local_var : m_all_local_vars) {
			local_var->name = m_def_provider->get_fresh_name(local_var->name);
		}
		// rename all local references with their new passive_var names
		for(auto & local_ref : m_all_local_references) {
			auto declaration = local_ref->get_declaration();
			if(!declaration) continue;
			declaration->is_used = true;
			if(local_ref->is_local) {
				local_ref->name = declaration->name;
			}
		}
		return true;
	}

	bool clear_passive_vars() {
		m_passive_vars_map.clear();
		return true;
	}

	bool clear_all_maps() {
		m_all_callback_decl.clear();
		m_all_local_vars.clear();
		m_passive_vars_map.clear();
		m_def_provider->refresh_scopes();
		return true;
	}

	bool is_thread_safe_env() const {
		return (m_program->current_callback and m_program->current_callback->is_thread_safe) or
			(m_program->get_curr_function() and m_program->get_curr_function()->is_thread_safe);
	}

	/// removes array declarations and deletes them under certain circumstances:
	/// - if they were promoted or are return vars
	static std::unique_ptr<NodeAST> to_assign_statement(NodeSingleDeclaration& node) {
		if(node.kind == NodeSingleDeclaration::Kind::Promoted or node.kind == NodeSingleDeclaration::Kind::ReturnVar) {
			return std::make_unique<NodeDeadCode>(node.tok);
		}
		auto node_assignment = node.to_assign_stmt();
		// if (const auto array_ref = node_assignment->l_value->cast<NodeArrayRef>()) {
		// 	if (!array_ref->index) {
		// 		//				return std::move(node_assignment);
		// 		return std::make_unique<NodeDeadCode>(node.tok);
		// 	}
		// }
		return std::move(node_assignment);
	}

private:
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		// update because of potentially altered param counts after lambda lifting
		node.update_function_lookup();
		m_def_provider->refresh_scopes();
		clear_passive_vars();

		m_program->global_declarations->accept(*this);
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_program->current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeBlock &node) override {
		m_current_block.push(&node);
		if(node.scope) {
			m_def_provider->add_scope();
			m_passive_vars_replace.emplace_back();
		}
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		if(node.scope) {
			const auto new_passive_vars = m_def_provider->remove_scope();
			// only add new passive vars on thread_safe callbacks and functions not using 'wait'
			// if(is_thread_safe_env()) {
				// add free memory vars which dynamic extent has ended to passive_vars vector
				add_passive_vars(new_passive_vars);
			// }
			// add used vars to passive_vars map
			const auto passive_used_vars = m_used_passive_vars[get_current_block()];
			for(auto & passive_var : passive_used_vars) {
				m_passive_vars_map[get_passive_var_hash(*passive_var)].push_back(passive_var);
			}
			// clear passive_var replace map
			m_passive_vars_replace.pop_back();
		}
		m_current_block.pop();
		node.flatten();
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		// if (auto definition = node.get_definition()) {
		// 	node.determine_function_strategy(m_program, m_program->current_callback);
		// }
		// do not visit definition -> because passive var allocation is separate between callbacks and functions
//		node.do_param_promotion(m_program);
		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
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

	static bool is_in_global_declarations(const NodeAST& node, NodeProgram* program) {
		return node.get_parent_block() == program->global_declarations.get();
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->determine_locality(m_program, get_current_block());

		if(is_in_global_declarations(node, m_program)) {
			node.variable->to_global();
			if(node.value) node.value->accept(*this);
			m_def_provider->set_declaration(node.variable, !node.variable->is_local);
			return &node;
		}

		// constant local declarations shall not be reused. But still need to be renamed to avoid name clashes
		if (node.variable->is_local and node.variable->data_type == DataType::Const) {
			node.variable->to_global();
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


		if(node.variable->is_local) {
			// if(is_thread_safe_env()) {
				if (auto free_passive_var = get_free_passive_var(*node.variable)) {
					free_passive_var->num_reuses = free_passive_var->num_reuses + 1 + node.variable->num_reuses;
					m_used_passive_vars[get_current_block()].push_back(free_passive_var);
					m_passive_vars_replace.back().insert({node.variable->name, free_passive_var});
					auto replacement = to_assign_statement(node);
					// visit replacement (assign statement) to replace local var with passive_var
					replacement->accept(*this);
					return node.replace_with(std::move(replacement));
				}
			// }
			// add local vars w/o free_passive_var to lists for later renaming
			if(m_program->current_callback)
				m_all_callback_decl[m_program->current_callback].push_back(&node);
			m_all_local_vars.push_back(node.variable);
		}

		// only add var to local scope if it is not replaced by passive_var
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
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

		// connect promoted refs and their declarations
		if(!node.get_declaration()) {
			auto node_declaration = m_def_provider->get_declaration(node);
			if (!node_declaration) DefinitionProvider::throw_declaration_error(node).exit();

			node.match_data_structure(node_declaration);
		}
		return &node;
	}

	NodeAST * visit(NodeArray& node) override {
//		node.determine_locality(m_program, m_current_body.top());

		if(node.size) node.size->accept(*this);

		m_def_provider->set_declaration(node.get_shared(), !node.is_local);
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
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
		// connect promoted refs and their declarations
		if(!node.get_declaration()) {
			auto node_declaration = m_def_provider->get_declaration(node);
			if (!node_declaration) {
				DefinitionProvider::throw_declaration_error(node).exit();
			}

			node.match_data_structure(node_declaration);
		}
		return &node;
	}

	NodeAST* visit(NodeVariable& node) override {
//		node.determine_locality(m_program, m_current_body.top());
		m_def_provider->set_declaration(node.get_shared(), !node.is_local);
		return &node;
	}

private:
	DefinitionProvider* m_def_provider;
	std::string loc_var_prefix = "loc_";
	std::stack<NodeBlock*> m_current_block;
	[[nodiscard]] NodeBlock* get_current_block() const {
		if (m_current_block.empty()) return nullptr;
		return m_current_block.top();
	}
	/// vector for all local declarations in callbacks
	std::unordered_map<NodeCallback*, std::vector<NodeSingleDeclaration*>> m_all_callback_decl = {};
	/// vector for all local vars in functions -> do not get moved into on init
	std::vector<std::shared_ptr<NodeDataStructure>> m_all_local_vars = {};
	/// hash values are the types
	std::unordered_map<std::size_t, std::vector<std::shared_ptr<NodeDataStructure>>> m_passive_vars_map;
	std::unordered_map<NodeBlock*, std::vector<std::shared_ptr<NodeDataStructure>>> m_used_passive_vars;
	void add_passive_vars(const std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>, StringHash, StringEqual>& map2) {
		for(auto & var : map2) {
			if(var.second->data_type == DataType::Mutable)
				m_passive_vars_map[get_passive_var_hash(*var.second)].push_back(var.second);
		}
	};

	/// get next free passive_var for given type
	std::shared_ptr<NodeDataStructure> get_free_passive_var(NodeDataStructure& data) {
		const auto hash = get_passive_var_hash(data);
		auto &vec = m_passive_vars_map[hash];
		if(vec.empty()) return nullptr;
		auto free_var = vec.back();
		vec.pop_back();
		return free_var;
	}
	/// search for new declaration to reference if declaration is replaced by passive_var
	std::shared_ptr<NodeDataStructure> get_new_declaration(const std::string& ref_name) {
		for (auto rit = m_passive_vars_replace.rbegin(); rit != m_passive_vars_replace.rend(); ++rit) {
			auto it = rit->find(ref_name);
			if (it != rit->end()) {
				return it->second;
			}
		}
		return nullptr;
	}
	/// constructs the hash to identify available passive vars h(type, size, thread_safety)
	// static std::string get_passive_var_hash(NodeDataStructure& data) {
	// 	auto hash = data.ty->to_string();
	// 	// add size if it is array
	// 	if(const auto array = data.cast<NodeArray>()) {
	// 		if(array->size) hash += array->size->get_string();
	// 	}
	// 	hash += data.is_thread_safe;
	// 	if(data.persistence.has_value()) hash += data.persistence.value().val;
	// 	return hash;
	// }

	static std::size_t get_passive_var_hash(NodeDataStructure& data) {
		std::size_t seed = 0;
		auto hashCombine = [&seed]<typename T0>(const T0& value) {
			std::hash<std::decay_t<T0>> hasher;
			seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
		};

		hashCombine(data.ty->to_string());

		// in case of array -> add sizes
		if (const auto array = data.cast<NodeArray>()) {
			if (array->size) hashCombine(array->size->get_string());
		} else if (const auto ndarray = data.cast<NodeNDArray>()) {
			if (ndarray->sizes) hashCombine(ndarray->sizes->get_string());
		}

		hashCombine(data.is_thread_safe);

		if (data.persistence.has_value()) hashCombine(data.persistence.value().val);

		return seed;
	}

	/// map for old datastructure name (as keys) that get replaced by new datastructures (passive_vars) (as values)
	std::vector<std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>>> m_passive_vars_replace;
	/// vector for all local references that have been replaced by passive_var references
	std::vector<NodeReference*> m_all_local_references;

};
