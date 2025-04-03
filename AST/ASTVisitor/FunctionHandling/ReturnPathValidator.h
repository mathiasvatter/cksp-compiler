//
// Created by Mathias Vatter on 09.08.24.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * @brief Validates if all code paths in a function lead to a return statement.
 *
 * This visitor class analyzes function definitions to ensure proper return behavior by:
 * - Tracking return paths through blocks, if-statements, and return statements
 * - Maintaining a stack of blocks being processed
 * - Storing return path information for each block
 * -> stores a boolean value for each block, indicating if a return statement was found, moves
 *    this boolean value up the block stack
 *
 * It helps prevent issues like:
 * - Missing return statements in conditional branches
 * - Incomplete switch statement return coverage
 */
class ReturnPathValidator: public ASTVisitor {
	std::stack<NodeBlock*> m_block_stack;
	std::unordered_map<NodeBlock*, bool> m_return_path;
public:

	bool do_return_path_validation(NodeFunctionDefinition& def) {
		// no need for validation if no return statements
		if (def.return_stmts.empty()) {
			return true;
		}
		def.accept(*this);
		if (!m_return_path[def.body.get()]) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::CompileError, *def.body);
			error.m_message = "Function <"+def.header->name+"> does not return a value on all code paths. This might lead to an infinite loop. "
					 "Check if:\n- All if/else branches have return statements\n- The function has a return statement after loops or conditionals\n- All possible cases in switch statements return a value.";
			error.print();
		}
		return m_return_path[def.body.get()];
	}

private:

	NodeAST* visit(NodeFunctionDefinition &node) override {
		m_return_path[node.body.get()] = true;
		node.body->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		m_block_stack.push(&node);
		for (const auto &stmt : node.statements) {
			stmt->accept(*this);
		}
		m_block_stack.pop();
		if (!m_block_stack.empty()) {
			m_return_path[m_block_stack.top()] &= m_return_path[&node];
		}
		return &node;
	}

	NodeAST* visit(NodeIf &node) override {
		// if the branches are empty, the return path is false
		m_return_path[node.if_body.get()] = !node.if_body->empty();
		node.if_body->accept(*this);

		m_return_path[node.else_body.get()] = !node.else_body->empty();
		node.else_body->accept(*this);
		// if both branches are true, the current block stays true
		m_return_path[m_block_stack.top()] &= (m_return_path[node.if_body.get()] and m_return_path[node.else_body.get()]);
		return &node;
	}

	NodeAST* visit(NodeSelect &node) override {
		bool all_return = true;
		for(const auto &cas: node.cases) {
			m_return_path[cas.second.get()] = !cas.second->empty();
			cas.second->accept(*this);
			all_return &= m_return_path[cas.second.get()];
		}
		m_return_path[m_block_stack.top()] &= all_return;

		return &node;
	}

	NodeAST* visit(NodeReturn &node) override {
		m_return_path[m_block_stack.top()] = true;
		return &node;
	}



};