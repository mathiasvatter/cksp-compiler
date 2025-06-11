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
				definition->is_used = true;
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

		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if (!m_program->function_call_stack.empty()) {
			if (auto decl = node.get_declaration()) {
				if (!decl->is_engine) return &node;
				const auto func_above = m_program->function_call_stack.top().lock();
				func_above->is_restricted |= decl->is_restricted;
			}
		}
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if (node.index) node.index->accept(*this);

		if (!m_program->function_call_stack.empty()) {
			if (auto decl = node.get_declaration()) {
				if (!decl->is_engine) return &node;
				const auto func_above = m_program->function_call_stack.top().lock();
				func_above->is_restricted |= decl->is_restricted;
			}
		}
		return &node;
	}

};

/**
 * Marks all variables in the program as thread unsafe if they are used in a non-thread-safe
 * environment. Will not visit callbacks or functions that are thread safe.
 */
class MarkThreadSafeVars final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

public:
	explicit MarkThreadSafeVars(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* mark_variables(NodeProgram& node) {
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

	[[nodiscard]] bool is_thread_safe_env() const {
		return (m_program->current_callback and m_program->current_callback->is_thread_safe) or // in callback
		(m_program->get_curr_function() and m_program->get_curr_function()->is_thread_safe) or // in function
		(!m_program->current_callback and !m_program->get_curr_function()); // global declarations
	}

	bool determine_thread_safety(NodeDataStructure& node) const {
		// the following data types can be thread unsafe and might need to be handled
		static const std::unordered_set<DataType> thread_unsafe_data_types = {
			DataType::Mutable, DataType::Return, DataType::Param
		};
		if (!is_thread_safe_env()) {
			if (thread_unsafe_data_types.contains(node.data_type)) {
				node.is_thread_safe = false;
			}
		}
		return node.is_thread_safe;
	}


private:

	NodeAST* visit(NodeCallback &node) override {
		if (node.is_thread_safe) {
			return &node;
		}
		m_program->current_callback = &node;
		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		m_program->current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		auto const definition = node.get_definition();
		if(node.kind == NodeFunctionCall::UserDefined and definition) {
			if(!definition->visited) {
				definition->visited = true;
				// if function is thread safe, all sub functions are also thread safe and
				// there is no need to mark their variables as thread unsafe
				if (definition->is_thread_safe) {
					return &node;
				}
				m_program->function_call_stack.push(definition);
				definition->accept(*this);
				m_program->function_call_stack.pop();
			}
		}

		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (node.value) node.value->accept(*this);
		// node.variable->is_thread_safe = is_thread_safe_env();
		determine_thread_safety(*node.variable);
		return &node;
	}

	NodeAST* visit(NodeFunctionParam& node) override {
		if (node.value) node.value->accept(*this);
		// node.variable->is_thread_safe = is_thread_safe_env();
		determine_thread_safety(*node.variable);
		return &node;
	}

};