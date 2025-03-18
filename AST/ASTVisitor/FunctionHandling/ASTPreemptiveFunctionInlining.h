//
// Created by Mathias Vatter on 09.08.24.
//

#pragma once

#include "../ASTVisitor.h"

/// Immediate Inlining of return-only functions
/// Substitution already takes place in the parent class ASTFunctionInlining in the following nodes:
/// NodeArrayRef
/// NodeNDArrayRef
/// NodeVariableRef
/// NodeFunctionHeader
class ASTPreemptiveFunctionInlining final : public ASTVisitor {
	DefinitionProvider *m_def_provider;

public:
	explicit ASTPreemptiveFunctionInlining(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	// static bool do_preemptive_function_inlining(const NodeFunctionCall& call) {
	// 	return call.strategy == NodeFunctionCall::Strategy::PreemptiveInlining
	// 	or call.strategy == NodeFunctionCall::Strategy::ExpressionFunc;
	// }

private:
	NodeAST* visit(NodeProgram &node) override {
		node.reset_function_used_flag();
		node.reset_function_visited_flag();
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		node.remove_unused_functions();
		return &node;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;
		if (node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		m_program->current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		if(node.bind_definition(m_program)) {
			if(node.is_builtin_kind()) return &node;
			auto definition = node.get_definition();
			m_program->function_call_stack.push(definition);
			if(!definition->visited) definition->accept(*this);
			definition->visited = true;

			node.determine_function_strategy(m_program, m_program->current_callback);
			// see if the function is a return-only function
			if(node.strategy == NodeFunctionCall::Strategy::PreemptiveInlining
				or node.strategy == NodeFunctionCall::Strategy::ExpressionFunc) {
				definition->is_used = false;
//				node.do_param_promotion(m_program);
				const auto new_node = node.do_function_inlining(m_program);
				m_program->function_call_stack.pop();
				return new_node;
			} else {
				definition->is_used = true;
				m_program->function_call_stack.pop();
			}
		}
		return &node;
	}

	// static inline bool is_initializer_function(const NodeFunctionCall& call) {
	// 	for(auto &arg : call.function->args->params) {
	// 		if(arg->cast<NodeInitializerList>()) {
	// 			return true;
	// 		}
	// 	}
	// 	return false;
	// }
	//
	// static inline bool is_wildcard_function(const NodeFunctionCall& call) {
	// 	for(auto &arg : call.function->args->params) {
	// 		if(auto nd_arg = arg->cast<NodeNDArrayRef>()) {
	// 			if(nd_arg->num_wildcards())
	// 				return true;
	// 		}
	// 	}
	// 	return false;
	// }

	NodeAST * visit(NodeFunctionHeaderRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.get_declaration() and node.is_func_arg()) {
			if(const auto def = node.get_declaration()->parent->cast<NodeFunctionDefinition>()) {
				def->is_used = true;
				def->accept(*this);
				def->visited = true;
			}
		}
		if(node.args) node.args->accept(*this);
		return &node;
	}

};