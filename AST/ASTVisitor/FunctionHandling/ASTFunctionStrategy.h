//
// Created by Mathias Vatter on 14.03.25.
//

#pragma once

#include "../ASTVisitor.h"

class ASTFunctionStrategy final : public ASTVisitor {
	DefinitionProvider *m_def_provider;
	NodeCallback* m_current_callback = nullptr;

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

private:
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(const auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		return &node;
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

			decide_by_ref_or_value(*definition->header, *node.function);

			const bool is_callable_env = m_current_callback != m_program->init_callback and !definition->is_restricted;

			if (definition->is_expression_function()) {
				node.strategy = NodeFunctionCall::Strategy::ExpressionFunc;
			} else if (is_initializer_function(node) or is_wildcard_function(node)) {
				node.strategy = NodeFunctionCall::Strategy::PreemptiveInlining;
			} else if (is_callable_env and node.function->has_no_args()) {
				node.strategy = NodeFunctionCall::Strategy::Call;
			} else if (is_callable_env and is_parameterstack_candidate(*definition)) {
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

	static void decide_by_ref_or_value(const NodeFunctionHeader& header, const NodeFunctionHeaderRef& header_ref) {
		for (size_t i = 0; i < header.get_num_params(); i++) {
			auto& formal_param = header.params[i];
			auto& actual_param = header_ref.get_arg(i);

			if (formal_param->variable->ty->cast<CompositeType>()) {
				formal_param->is_pass_by_ref = true;
			}
			if (formal_param->variable->data_type == DataType::UIControl) {
				formal_param->is_pass_by_ref = true;
			}

			// if (actual_param->cast<NodeInitializerList>()) {
			// 	formal_param->is_pass_by_ref = false;
			// }

		}
	}


	static bool is_parameterstack_candidate(const NodeFunctionDefinition& def) {
		for(const auto &param : def.header->params) {
			if (param->variable->ty->is_union_type() || param->variable->data_type == DataType::UIControl ||
				param->is_pass_by_ref) {
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