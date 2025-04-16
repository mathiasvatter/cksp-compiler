//
// Created by Mathias Vatter on 11.03.25.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * marks functions and callbacks for their thread safety and their restrictiveness
 */
class MarkThreadSafe final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

public:
	explicit MarkThreadSafe(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* mark_environments(NodeProgram& node) {
		m_program = &node;
		m_program->current_callback = nullptr;
		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* mark_function(NodeFunctionDefinition& node) {
		node.visited = false;
		m_program->current_callback = nullptr;
		m_program->function_call_stack.push(node.get_shared());
		node.accept(*this);
		m_program->function_call_stack.pop();
		m_program->reset_function_visited_flag();
		return &node;
	}

private:

	NodeAST* visit(NodeCallback &node) override {
		m_program->current_callback = &node;
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		m_program->current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		node.bind_definition(m_program);
		auto const definition = node.get_definition();
		if(node.kind == NodeFunctionCall::UserDefined and definition) {
			if(!definition->visited) {
				m_program->function_call_stack.push(definition);
				definition->accept(*this);
				m_program->function_call_stack.pop();
				definition->visited = true;
			}
		}

		// determine thread safety of currently visiting function definition
		if(definition) {
			if (!m_program->function_call_stack.empty()) {
				const auto func_above = m_program->function_call_stack.top().lock();
				func_above->is_thread_safe &= definition->is_thread_safe;
				func_above->is_restricted |= definition->is_restricted;
			}
			if(m_program->current_callback) {
				m_program->current_callback->is_thread_safe &= definition->is_thread_safe;
			}
		}

		// node.function->accept(*this);
		return &node;
	}





};