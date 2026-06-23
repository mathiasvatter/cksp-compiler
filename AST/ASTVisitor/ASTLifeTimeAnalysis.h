//
// Created by Mathias Vatter on 04.06.26.
//

#pragma once

#include "ASTVisitor.h"

/**
 * This pass analysis lifetimes of variables by using Life structs with the first statement
 * and the last statement they are used in.
 * It parses stmt by stmt, assigns a lifetime object with the current stmt as start and end
 * upon visiting a declaration and assignes a new end stmt every time a reference to this var is
 * visited
 * - variables declared in a while loop (since all other loop forms are transformed at this point)
 *   are always assigned the full lifetime of the while loop body
 */
class ASTLifeTimeAnalysis : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	NodeStatement* m_current_statement = nullptr;
	struct Life {
		NodeStatement* start;
		NodeStatement* end;
		/// checks if none of the members are nullptr
		[[nodiscard]] bool is_valid() const {
			return start != nullptr and end != nullptr;
		}
		/// checks if variable life ends immediately after it starts -> dead storage
		bool is_unused() const {
			return is_valid() and start == end;
		}
	};
	std::unordered_map<NodeDataStructure*, Life> m_life_times;
	std::unordered_set<NodeBlock*> m_visited_blocks;
	bool is_in_while_loop = false;
	std::stack<NodeStatement*> m_end_of_current_block{};
	std::unordered_set<Life*> m_variables_in_while;

	void add_lifetime_end(const NodeReference& ref, NodeStatement* stmt) {
		// a variable for sure -> the end will then be the end of the scope it was declared in.
		if (const auto data = ref.get_declaration()) {
			const auto it = m_life_times.find(data.get());
			if (it != m_life_times.end()) {
				if (is_in_while_loop) {
					it->second.end = m_end_of_current_block.top();
				} else {
					it->second.end = stmt;
				}
			}
		}
	}

	void add_lifetime_start(NodeDataStructure* data, NodeStatement* stmt) {
		if (data->is_local) {
			m_life_times[data] = {stmt, stmt};
		}
	}

public:
	explicit ASTLifeTimeAnalysis(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	std::unordered_map<NodeDataStructure*, Life>& run(NodeBlock& block) {
		m_variables_in_while.clear();
		is_in_while_loop = false;
		m_life_times.clear();
		m_visited_blocks.clear();
		block.accept(*this);
		return m_life_times;
	}

	// this is not used in variable reuse because all relevant function parameters are already
	// transformed into assignment statements by parameter transform pass
	std::unordered_map<NodeDataStructure*, Life>& run(NodeFunctionDefinition& def) {
		m_variables_in_while.clear();
		is_in_while_loop = false;
		m_life_times.clear();
		m_visited_blocks.clear();
		m_current_statement = def.body->front();
		def.accept(*this);
		return m_life_times;
	}

	/// can be called after run() -> deletes all declaration nodes with variables where
	/// start and end member in Life are the same
	int remove_unused_local_variables() {
		int num_unused = 0;
		for (auto & [data, life] : m_life_times) {
			if (life.is_unused()) {
				if (auto decl = data->parent->cast<NodeSingleDeclaration>()) {
					if (decl->value and decl->value->cast<NodeFunctionCall>()) continue; // do not delete if r_value is func call
					decl->remove_node();
					num_unused++;
				}
			}
		}
		return num_unused;
	}

	const Life *get_lifetime(NodeDataStructure *data) const {
		const auto it = m_life_times.find(data);
		if (it != m_life_times.end()) {
			auto l = &it->second;
			if (l->is_valid()) {
				return l;
			}
		}
		return nullptr;
	}

protected:

	// inside a while loop, variables cannot have their end of life, they are saved in a map
	// and are assigned their lifetime end in the next statement
	NodeAST* visit(NodeWhile &node) override {
		is_in_while_loop = true;
		ASTVisitor::visit(node);
		is_in_while_loop = false;
		return &node;
	}

	NodeAST *visit(NodeBlock &node) override {
		m_visited_blocks.insert(&node);
		m_end_of_current_block.push(node.back());
		ASTVisitor::visit(node);
		m_end_of_current_block.pop();
		return &node;
	}

	NodeAST *visit(NodeStatement &node) override {
		m_current_statement = &node;
		node.statement->accept(*this);
		m_current_statement = nullptr;
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (const auto ui_control = node.variable->cast<NodeUIControl>()) {
			ui_control->control_var->to_global();
		}
		if (node.variable->data_type == DataType::Const and node.variable->is_local) {
			node.variable->to_global();
		}

		// if node variable is local, it starts its life -> add to alive_vars set
		add_lifetime_start(node.variable.get(), m_current_statement);

		if (node.value) {
			node.value->accept(*this);
		}

		return &node;
	}

	NodeAST* visit(NodeFunctionParam & node) override {
		node.variable->accept(*this);
		// have to do this extra if-check because function param nodes are also used for
		// foreach key, value
		if (node.variable->is_function_param()) {
			add_lifetime_start(node.variable.get(), m_current_statement);
		}
		if (node.value) node.value->accept(*this);
		return &node;
	}

	NodeAST *visit(NodeArrayRef &node) override {
		add_lifetime_end(node, m_current_statement);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeVariableRef& node) override {
		add_lifetime_end(node, m_current_statement);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		add_lifetime_end(node, m_current_statement);
		return ASTVisitor::visit(node);
	}

	NodeAST* visit(NodePointerRef& node) override {
		add_lifetime_end(node, m_current_statement);
		return ASTVisitor::visit(node);
	}

	// NodeAST* visit(NodeFunctionCall& node) override {
	// 	node.function->accept(*this);
	// 	node.bind_definition(m_program);
	// }






};
