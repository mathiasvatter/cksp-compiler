//
// Created by Mathias Vatter on 08.06.26.
//

#pragma once
#include "ASTLifeTimeAnalysis.h"
#include "ASTVisitor.h"

/**
 * Marks all variables in the program as thread unsafe if they are used in a non-thread-safe
 * environment. This is determined by thread unsafe ranges (ranges from function or callback
 * start until the first asynchronous (wait) builtin command or until the next wait command
 */
class ASTThreadSafeVariableMarking : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

	bool m_is_thread_safe_environment = true;
	NodeStatement* m_start = nullptr;
	NodeStatement* m_end = nullptr;
	ASTLifeTimeAnalysis m_lifetime_analysis;
	std::unordered_map<NodeStatement*, std::vector<NodeDataStructure*>> m_end_of_life;

	bool determine_thread_safety(NodeDataStructure& node) const {
		// the following data types can be thread unsafe and might need to be handled
		const bool is_potentially_thread_unsafe =
			node.data_type == DataType::Mutable or
			node.data_type == DataType::Return or
			node.data_type == DataType::Param;
		if (!m_is_thread_safe_environment and is_potentially_thread_unsafe) {
			node.is_thread_safe = false;
		}
		return node.is_thread_safe;
	}

	bool add_end_of_life(NodeDataStructure* data) {
		if (auto l = m_lifetime_analysis.get_lifetime(data)) {
			m_end_of_life[l->end].push_back(data);
			return true;
		}
		return false;
	}

public:
	explicit ASTThreadSafeVariableMarking(NodeProgram* main) : m_def_provider(main->def_provider), m_lifetime_analysis(main) {
		m_program = main;
	}

	NodeAST* run(NodeCallback& node, NodeStatement* start, NodeStatement* end) {
		m_lifetime_analysis.run(*node.statements);
		m_end_of_life.clear();
		m_start = start;
		m_end = end;
		m_is_thread_safe_environment = false;
		return node.accept(*this);
	}

	NodeAST* run(NodeFunctionDefinition& node, NodeStatement* start, NodeStatement* end) {
		m_lifetime_analysis.run(node);
		m_end_of_life.clear();
		m_start = start;
		m_end = end;
		m_is_thread_safe_environment = false;
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
			// check if any vars lifetime ends here -> if we are still in thread-unsafe env -> var is thread-safe
			if (!m_is_thread_safe_environment) {
				auto it = m_end_of_life.find(stmt.get());
				if (it != m_end_of_life.end()) {
					for (auto & data : it->second) {
						data->is_thread_safe = true;
					}
				}
			}
		}
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		determine_thread_safety(*node.variable);
		add_end_of_life(node.variable.get());
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeFunctionParam& node) override {
		determine_thread_safety(*node.variable);
		add_end_of_life(node.variable.get());
		return ASTVisitor::visit(node);
	}

};

/**
 * Adds ThreadUnsafeRange objects with a start and end statement to make thread safe analysis
 * of variables more fine-grained.
 * Every callback and every function definition get a range assigned where the thread unsafe range
 * is -> inside those ranges, variables will be marked as thread unsafe
 */
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
	std::unordered_map<NodeFunctionDefinition*, ThreadUnsafeRange> m_function_thread_unsafe_ranges;
	std::unordered_map<NodeCallback*, ThreadUnsafeRange> m_callback_thread_unsafe_ranges;

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
		m_function_thread_unsafe_ranges.reserve(node.function_definitions.size());
		m_callback_thread_unsafe_ranges.reserve(node.callbacks.size());
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	/// can be called after run()
	void mark_variables() {
		// parallel_for_each(m_function_thread_unsafe_ranges.begin(), m_function_thread_unsafe_ranges.end(),
		// 	[&](auto& pair) {
		// 		auto& [func, range] = pair;
		// 		if (range->is_valid() and !range->is_thread_safe) {
		// 			const auto variable_marking = std::make_unique<ASTThreadSafeVariableMarking>(m_program);
		// 			variable_marking->run(*func, range->start, range->end);
		// 		}
		// 	}
		// );
		for (auto& [func, range] : m_function_thread_unsafe_ranges) {
			if (range.is_valid() and !range.is_thread_safe) {
				m_variable_marking.run(*func, range.start, range.end);
			}
		}
		// parallel_for_each(m_callback_thread_unsafe_ranges.begin(), m_callback_thread_unsafe_ranges.end(),
		// 	[&](auto& pair) {
		// 		auto& [cb, range] = pair;
		// 		if (range->is_valid() and !range->is_thread_safe) {
		// 			const auto variable_marking = std::make_unique<ASTThreadSafeVariableMarking>(m_program);
		// 			variable_marking->run(*cb, range->start, range->end);
		// 		}
		// 	}
		// );
		for (auto& [cb, range] : m_callback_thread_unsafe_ranges) {
			if (range.is_valid() and !range.is_thread_safe) {
				m_variable_marking.run(*cb, range.start, range.end);
			}
		}
	}

	void print() {
		for (auto& [cb, range] : m_callback_thread_unsafe_ranges) {
			std::cout << range.start->tok.file << ":" << range.start->tok.line << " - " << range.end->tok.file << ":" << range.end->tok.line << std::endl;
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
		m_callback_thread_unsafe_ranges[&node] = ThreadUnsafeRange{true, first_stmt, last_stmt};
		// auto & r = m_callback_thread_unsafe_ranges[&node];
		node.statements->accept(*this);
		m_current_callback = nullptr;
		return &node;
	}

	NodeAST *visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		// node.bind_definition(m_program);
		auto const definition = node.get_definition();

		// is wait() or some asynchronous builtin function
		if (definition) {
			if (!definition->is_thread_safe) {
				// we are in the function call stack
				if (auto curr_func = m_program->get_curr_function()) {
					auto& range = m_function_thread_unsafe_ranges[curr_func.get()];
					range.end = m_current_statement;
					range.is_thread_safe = definition->is_thread_safe;
				} else {
					// we are in a callback
					auto& range = m_callback_thread_unsafe_ranges[m_current_callback];
					range.end = m_current_statement;
					range.is_thread_safe = definition->is_thread_safe;
				}
			}
		}


		if(!node.is_builtin_kind() and definition) {
			if (definition and definition->body->empty()) return &node; // if function body is empty, there are no thread-unsafe statements to mark
			if(!definition->visited) {
				// important! function parameter are not marked here since we are only looking at statements
				// so there has to be taken extra care in later stages to mark them as thread unsafe if the
				// function definition is also thread unsafe
				m_function_thread_unsafe_ranges[definition.get()] = ThreadUnsafeRange{true,
					definition->body->statements.front().get(), definition->body->statements.back().get()
				};
				FunctionCallStackScope diagnostic_frame(*m_program, node);
				m_program->function_definition_stack.push(definition);
				definition->accept(*this);
				m_program->function_definition_stack.pop();
				definition->visited = true;
			}
		}


		return &node;
	}


};

