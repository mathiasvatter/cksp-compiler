//
// Created by Mathias Vatter on 26.05.25.
//

#pragma once
#include "../ASTVisitor.h"
#include "../GlobalScope/ASTVariableReuse.h"


class ParameterReuse final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	// those are all functions that get called within one function definition -> they depend on each other
	std::unordered_map<NodeFunctionDefinition*, std::unordered_set<NodeFunctionDefinition*>> m_subcalls_per_function;
	std::unordered_map<NodeFunctionDefinition*, std::vector<NodeDataStructure*>> m_param_decl_per_function;
	struct ParamFromFunction {
		NodeDataStructure* param = nullptr;
		std::unordered_set<NodeFunctionDefinition*> function{};
		ParamFromFunction(NodeDataStructure* param, std::unordered_set<NodeFunctionDefinition*> function)
		: param(param), function(std::move(function)) {}
	};
	std::unordered_map<std::size_t, std::vector<std::unique_ptr<ParamFromFunction>>> params_to_reuse{};

public:
	explicit ParameterReuse(NodeProgram* main) {
		m_program = main;
		m_def_provider = m_program->def_provider;
	}

	void do_parameter_reuse (
		const std::unordered_map<NodeFunctionDefinition*, std::unordered_set<NodeFunctionDefinition*>>& subcalls_per_function,
		const std::unordered_map<NodeFunctionDefinition*, std::vector<NodeDataStructure*>>& params_per_function
	) {
		m_subcalls_per_function = subcalls_per_function;
		m_param_decl_per_function = params_per_function;

		parameter_reuse();
		// clear all maps for next run
		params_to_reuse.clear();
	}


private:
	std::unique_ptr<ParamFromFunction> get_free_param(NodeDataStructure& param, NodeFunctionDefinition& def) {
		auto hash = ASTVariableReuse::get_passive_var_hash(param);
		auto it = params_to_reuse.find(hash);
		if (it == params_to_reuse.end()) return nullptr;

		auto& reuse_list = it->second;
		if (reuse_list.empty()) return nullptr;

		for (int i = reuse_list.size() - 1; i >= 0; --i) {
			auto& reuse = reuse_list[i];

			bool conflict = false;
			// check if current function is dependent on the function that declared the param
			for (auto dependent_func : reuse->function) {
				if (function_is_subcall(&def, dependent_func)) {
					conflict = true;
					break;
				}
			}

			if (!conflict) {
				// found a param that can be reused
				auto reuse_param = std::move(reuse);
				reuse_param->param->num_reuses++;
				reuse_list.erase(reuse_list.begin() + i);
				return reuse_param;
			}
		}
		return nullptr;
	}

	void add_free_param(NodeDataStructure& param, NodeFunctionDefinition& def) {
		auto hash = ASTVariableReuse::get_passive_var_hash(param);
		std::unordered_set<NodeFunctionDefinition*> init_dependencies;
		init_dependencies.insert(&def);
		params_to_reuse[hash].push_back(std::make_unique<ParamFromFunction>(&param, init_dependencies));
	}

	void add_free_param(std::unique_ptr<ParamFromFunction> param) {
		auto hash = ASTVariableReuse::get_passive_var_hash(*param->param);
		params_to_reuse[hash].push_back(std::move(param));
	}

	bool function_is_subcall(NodeFunctionDefinition* caller, NodeFunctionDefinition* callee) const {
		if (!caller || !callee) return false;
		if (caller == callee) return true; // a function is always a subcall of itself
		if (auto it = m_subcalls_per_function.find(caller); it != m_subcalls_per_function.end()) {
			return it->second.contains(callee);
		}
		return false;
	}

	void parameter_reuse() {

		for (auto& def : m_program->function_definitions) {
			// check if there are params in declarations for this function
			auto it = m_param_decl_per_function.find(def.get());
			if (it == m_param_decl_per_function.end()) continue;

			auto& params = it->second;
			std::vector<std::unique_ptr<ParamFromFunction>> params_reused_in_function;
			// first, try to reuse the params from the function definition
			// get reusable param from map
			for (size_t i = 0; i < params.size(); i++) {
				auto & param = params[i];
				if (auto reused_param = get_free_param(*param, *def)) {
					auto reused = reused_param->param->get_shared();
					// add current function to param function dependencies
					reused_param->function.insert(def.get());
					params_reused_in_function.push_back(std::move(reused_param));
					// set new declaration pointer to reused param in param references
					parallel_for_each(param->references.begin(), param->references.end(),
					  [&reused](auto const& ref) {
						ref->declaration = reused;
					  	ref->name = reused->name;
					});

					reused->references.insert(param->references.begin(), param->references.end());
					const auto decl = param->parent->cast<NodeSingleDeclaration>();
					if (!decl) {
						auto error = CompileError(ErrorType::InternalError, "", "", param->tok);
						error.m_message = "ParameterReuse : Parameter declaration is not a NodeSingleDeclaration.";
						error.exit();
					}
					decl->remove_node();

				} else {
					add_free_param(*param, *def);
				}

			}

			for (size_t i = 0; i<params_reused_in_function.size(); i++) {
				add_free_param(std::move(params_reused_in_function[i]));
			}
			params_reused_in_function.clear();

		}

	}


};