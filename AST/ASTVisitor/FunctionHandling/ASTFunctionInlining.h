//
// Created by Mathias Vatter on 08.08.24.
//

#pragma once

#include "ParameterStackTransformation.h"
#include "../ASTVisitor.h"

class ASTFunctionInlining final : public ASTVisitor {

	DefinitionProvider *m_def_provider;
	NodeCallback* m_current_callback = nullptr;
public:
	explicit ASTFunctionInlining(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	/// check for used functions
	NodeAST *visit(NodeProgram &node) override {
		m_program = &node;

		// ASTFunctionStrategy strategy(m_program);
		// strategy.determine_function_strategies(*m_program);

		static ParameterStackTransformation transform(m_program);
		transform.do_function_stack_transformation(*m_program);
		// node.debug_print();

		node.reset_function_used_flag();
		node.reset_function_visited_flag();
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		node.remove_unused_functions();
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST *visit(NodeCallback &node) override {
		m_current_callback = &node;
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		m_current_callback = nullptr;
		return &node;
	}

	/// initiating substitution
	NodeAST *visit(NodeFunctionCall &node) override {
		// visit header
		node.function->accept(*this);
		// check if function is called with correct amount of arguments
		if(node.is_call and !node.function->has_no_args()) {
			// auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			// error.m_message = "Found incorrect amount of function arguments when using <call>.";
			// error.exit();
			node.is_call = false;
		}

		node.bind_definition(m_program);
		auto definition = node.get_definition();

		if(node.kind == NodeFunctionCall::Kind::Property) {
			CompileError(ErrorType::InternalError,"Found undefined property function.", "", node.tok).exit();
		}
		if(!definition) {
			// since we do depth first and try to visit every function before inlining,
			// skip the error throw if definition is not found at the first visit since it
			// could be found later -> high-order functions etc.
			if(node.function->get_declaration() and node.function->get_declaration()->is_function_param())
				return &node;

			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "Unable to find function definition for <"+node.function->name+">.";
			error.exit();
		}

		if(node.kind == NodeFunctionCall::Kind::Builtin) return &node;

		if(definition->is_restricted) {
			if(!contains(RESTRICTED_CALLBACKS, remove_substring(m_current_callback->begin_callback, "on "))) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message = "<"+node.function->name+"> can only be used in <on init>, <on persistence_changed>, <pgs_changed>, <on ui_control> callbacks.";
				error.m_got = "<"+m_current_callback->begin_callback+">";
				error.exit();
			}
		}

		// only threadsafe functions can be called in <on init> callback
		if(m_current_callback == m_program->init_callback) {
			if(!definition->is_thread_safe) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message = "Only threadsafe functions can be called in the <on init> callback. Function <"
								  +node.function->name+"> contains asychronous operations.";
				error.exit();
			}
			if(node.strategy == NodeFunctionCall::Strategy::Call) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message =
					"The usage of <call> keyword is not allowed in the <on init> callback. Automatically removed <call> and inlined function. Consider not using the <call> keyword.";
				error.print();
				node.strategy = NodeFunctionCall::Strategy::Inlining;
			}
		}

		m_program->function_call_stack.push(definition);
		// visit everything beforehand to get depth first search
		if(!definition->visited) {
			definition->accept(*this);
		}
		definition->visited = true;

		node.determine_function_strategy(m_program, m_current_callback);
		// if node is_call, do not inline and return
		if(node.strategy == NodeFunctionCall::Strategy::Call) {
			definition->is_used = true;
			m_program->function_call_stack.pop();
			node.is_call = true;
			return &node;
		}

		// used can not be set to false here, because some func calls might be "called" and some not
		auto new_node = node.do_function_inlining(m_program);
		m_program->function_call_stack.pop();
		return new_node;
	}

};
