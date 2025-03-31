//
// Created by Mathias Vatter on 14.03.25.
//

#pragma once

#include "../ASTVisitor.h"

class ASTFunctionStrategy final : public ASTVisitor {
	DefinitionProvider *m_def_provider;
	NodeCallback* m_current_callback = nullptr;

	/// stores for every restricted builtin function the callbacks where they are allowed
	/// purge_group can only be used in on ui_control, on ui_controls and on persistence_changed
	/// wait, wait_ticks, wait_async, stop_wait can not be used in on init
	/// save_array, save_array_str, load_array, load_array_str can be used in on init, on persistence_changed, on ui_control and on pgs_changed
	///
	inline static const std::unordered_map<std::string, std::unordered_set<std::string>> m_restricted_functions = {
		// purge_group: Nur in "ui_control", "ui_controls" und "persistence_changed" erlaubt
		{ "purge_group", { "ui_control", "ui_controls", "persistence_changed" } },

		// wait-Funktionen: Alle CALLBACKS außer "init"
		{ "wait", ([]() -> std::unordered_set<std::string> {
			 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
			 allowed.erase("init");
			 return allowed;
		   })() },
		{ "wait_ticks", ([]() -> std::unordered_set<std::string> {
			 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
			 allowed.erase("init");
			 return allowed;
		   })() },
		{ "wait_async", ([]() -> std::unordered_set<std::string> {
			 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
			 allowed.erase("init");
			 return allowed;
		   })() },
		{ "stop_wait", ([]() -> std::unordered_set<std::string> {
			 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
			 allowed.erase("init");
			 return allowed;
		   })() },

		// save_array und verwandte Funktionen: Erlaubt in "init", "persistence_changed", "ui_control" und "pgs_changed"
		{ "save_array",     { "init", "persistence_changed", "ui_control", "pgs_changed" } },
		{ "save_array_str", { "init", "persistence_changed", "ui_control", "pgs_changed" } },
		{ "load_array",     { "init", "persistence_changed", "ui_control", "pgs_changed" } },
		{ "load_array_str", { "init", "persistence_changed", "ui_control", "pgs_changed" } }
	};
public:
	explicit ASTFunctionStrategy(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* determine_function_strategies(NodeProgram& node) {
		m_current_callback = nullptr;
		node.reset_function_visited_flag();
		node.accept(*this);
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* determine_function_strategy(NodeFunctionCall& call, NodeCallback* callback) {
		m_current_callback = callback;
		call.accept(*this);
		m_current_callback = nullptr;
		return &call;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_current_callback = &node;
		if (node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		m_current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);

		if (node.is_builtin_kind()) return &node;

		if (const auto definition = node.get_definition()) {
			if (!definition->visited) definition->accept(*this);
			definition->visited = true;
			node.is_call = false;
			if (definition->is_expression_function()) {
				node.strategy = NodeFunctionCall::Strategy::ExpressionFunc;
			} else if (m_current_callback == m_program->init_callback or definition->is_restricted) {
				// restricted functions like purge_group etc cannot be in native functions
				node.strategy = NodeFunctionCall::Strategy::Inlining;
				if (is_initializer_function(node) or is_wildcard_function(node))
					node.strategy = NodeFunctionCall::Strategy::PreemptiveInlining;
			} else if (node.function->has_no_args()) {
				node.strategy = NodeFunctionCall::Strategy::Call;
			} else if (has_only_scalar_params(*definition)) {
				node.strategy = NodeFunctionCall::Strategy::ParameterStack;
			} else {
				node.strategy = NodeFunctionCall::Strategy::Inlining;
			}
		}

		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition &node) override {
		node.body->accept(*this);
		return &node;
	}

	static bool has_only_scalar_params(const NodeFunctionDefinition& def) {
		for(const auto &param : def.header->params) {
			if(param->variable->ty->cast<CompositeType>() || param->variable->ty->is_union_type()) {
				return false;
			}
		}
		return true;
	}

	/// function call has initializer list in its actual parameters
	static bool is_initializer_function(const NodeFunctionCall& call) {
		for(const auto &arg : call.function->args->params) {
			if(arg->cast<NodeInitializerList>()) {
				return true;
			}
		}
		return false;
	}

	/// function call has wildcards in its actual parameters
	static bool is_wildcard_function(const NodeFunctionCall& call) {
		for(const auto &arg : call.function->args->params) {
			if(const auto nd_arg = arg->cast<NodeNDArrayRef>()) {
				if(nd_arg->num_wildcards())
					return true;
			}
		}
		return false;
	}

};