//
// Created by Mathias Vatter on 08.06.26.
//

#pragma once
#include "ASTVisitor.h"

class ASTThreadSafeVariableMarking : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

	bool m_is_thread_safe_environment = true;
	NodeStatement* m_start = nullptr;
	NodeStatement* m_end = nullptr;

	bool determine_thread_safety(NodeDataStructure& node) const {
		// the following data types can be thread unsafe and might need to be handled
		static const std::unordered_set<DataType> thread_unsafe_data_types = {
			DataType::Mutable, DataType::Return, DataType::Param
		};
		if (!m_is_thread_safe_environment) {
			if (thread_unsafe_data_types.contains(node.data_type)) {
				node.is_thread_safe = false;
			}
		}
		return node.is_thread_safe;
	}

public:
	explicit ASTThreadSafeVariableMarking(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* run(NodeCallback& node, NodeStatement* start, NodeStatement* end) {
		m_start = start;
		m_end = end;
		m_is_thread_safe_environment = true;
		return node.accept(*this);
	}

	NodeAST* run(NodeFunctionDefinition& node, NodeStatement* start, NodeStatement* end) {
		m_start = start;
		m_end = end;
		m_is_thread_safe_environment = true;
		return node.accept(*this);
	}

protected:

	NodeAST *visit(NodeBlock &node) override {
		for (auto &stmt : node.statements) {
			if (stmt.get() == m_start) {
				m_is_thread_safe_environment = false;
			}
			stmt->accept(*this);
			if (stmt.get() == m_end) {
				m_is_thread_safe_environment = true;
			}
		}
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (node.value) node.value->accept(*this);
		determine_thread_safety(*node.variable);
		return &node;
	}

	NodeAST* visit(NodeFunctionParam& node) override {
		if (node.value) node.value->accept(*this);
		determine_thread_safety(*node.variable);
		return &node;
	}

};


class ASTThreadSafeAnalysis : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	NodeStatement* m_current_statement = nullptr;
	NodeCallback* m_current_callback = nullptr;

	ASTThreadSafeVariableMarking m_variable_marking;

	struct ThreadUnsafeRange {
		bool is_thread_safe = true;
		NodeStatement* start = nullptr;
		NodeStatement* end = nullptr;
		/// checks if none of the members are nullptr
		[[nodiscard]] bool is_valid() const {
			return start != nullptr and end != nullptr;
		}
	};

	std::stack<ThreadUnsafeRange*> m_thread_unsafe_ranges;
	std::unordered_map<NodeFunctionDefinition*, std::unique_ptr<ThreadUnsafeRange>> m_function_thread_unsafe_ranges;
	std::unordered_map<NodeCallback*, std::unique_ptr<ThreadUnsafeRange>> m_callback_thread_unsafe_ranges;

public:
	explicit ASTThreadSafeAnalysis(NodeProgram* main) : m_def_provider(main->def_provider), m_variable_marking(main) {
		m_program = main;
	}

	NodeAST* run(NodeProgram& node) {
		m_program = &node;
		m_current_callback = nullptr;
		m_current_statement = nullptr;
		m_function_thread_unsafe_ranges.clear();
		m_callback_thread_unsafe_ranges.clear();
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	/// can be called after run()
	void mark_variables() {
		for (auto& [func, range] : m_function_thread_unsafe_ranges) {
			if (range->is_valid() and !range->is_thread_safe) {
				m_variable_marking.run(*func, range->start, range->end);
			}
		}
		for (auto& [cb, range] : m_callback_thread_unsafe_ranges) {
			if (range->is_valid() and !range->is_thread_safe) {
				m_variable_marking.run(*cb, range->start, range->end);
			}
		}
	}

	void print() {
		for (auto& [cb, range] : m_callback_thread_unsafe_ranges) {
			// if ()
			auto& r = range;
			std::cout << range->start->tok.file << ":" << range->start->tok.line << " - " << range->end->tok.file << ":" << range->end->tok.line << std::endl;
		}
	}

protected:

	NodeAST *visit(NodeStatement &node) override {
		m_current_statement = &node;
		node.statement->accept(*this);
		m_current_statement = nullptr;
		return &node;
	}

	NodeAST *visit(NodeCallback &node) override {
		if (node.statements->empty()) return &node;
		m_current_callback = &node;
		const auto first_stmt = node.statements->front();
		const auto last_stmt = node.statements->back();
		m_callback_thread_unsafe_ranges[&node] = std::make_unique<ThreadUnsafeRange>(true, first_stmt, last_stmt);
		auto & r = m_callback_thread_unsafe_ranges[&node];
		node.statements->accept(*this);
		m_current_callback = nullptr;
		return &node;
	}

	NodeAST *visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		node.bind_definition(m_program);
		auto const definition = node.get_definition();

		// is wait() or some asynchronous builtin function
		if (definition) {
			if (!definition->is_thread_safe) {
				// we are in the function call stack
				if (auto curr_func = m_program->get_curr_function()) {
					auto& range = m_function_thread_unsafe_ranges[curr_func.get()];
					range->end = m_current_statement;
					range->is_thread_safe = definition->is_thread_safe;
				} else {
					// we are in a callback
					auto& range = m_callback_thread_unsafe_ranges[m_current_callback];
					range->end = m_current_statement;
					range->is_thread_safe = definition->is_thread_safe;
				}
			}
		}


		if(!node.is_builtin_kind() and definition) {
			if (definition and definition->body->empty()) return &node; // if function body is empty, there are no thread-unsafe statements to mark
			if(!definition->visited) {
				m_function_thread_unsafe_ranges[definition.get()] = std::make_unique<ThreadUnsafeRange>(true,
					definition->body->statements.front().get(), definition->body->statements.back().get()
				);
				m_program->function_call_stack.push(definition);
				definition->accept(*this);
				m_program->function_call_stack.pop();
				definition->visited = true;
			}
		}


		return &node;
	}


};


