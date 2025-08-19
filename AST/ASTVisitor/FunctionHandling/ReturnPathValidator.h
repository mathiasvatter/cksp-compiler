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
class ReturnPathValidator final : public ASTVisitor {
	std::stack<NodeBlock*> m_block_stack;
	std::unordered_map<NodeBlock*, bool> m_return_path;
public:

	bool do_return_path_validation(NodeFunctionDefinition& def) {
		m_return_path.clear();
		// no need for validation if no return statements
		if (def.return_stmts.empty()) {
			return true;
		}
		def.accept(*this);
		if (!m_return_path[def.body.get()]) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::CompileError, *def.body);
			error.m_message = "Function <"+def.header->name+"> does not return a value on all code paths. This might lead to an infinite loop. "
					 "Check if:\n- All if/else branches have return statements\n- The function has a return statement after loops or conditionals\n- All possible cases in switch statements return a value.";

			error.m_message += "\n Possible missing return statements in lines: ";
			std::vector<size_t> lines{};
			for (auto& [block, boolean] : m_return_path) {
				if (!boolean and !block->empty()) {
					lines.push_back(block->range.end.line);
				}
			}
			error.m_message += StringUtils::join_apply(lines, [](size_t line) {
				return std::to_string(line);
			}, ", ");

			error.exit();
		}

		return m_return_path[def.body.get()];
	}

private:

	NodeAST* visit(NodeFunctionDefinition &node) override {
		// m_return_path[node.body.get()] = true;
		node.body->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		m_return_path[&node] = false;
		m_block_stack.push(&node);
		for (const auto &stmt : node.statements) {
			stmt->accept(*this);
			if (m_return_path[&node]) {
				break;
			}
		}
		m_block_stack.pop();
		// if (!m_block_stack.empty()) {
		// 	m_return_path[m_block_stack.top()] &= m_return_path[&node];
		// }
		return &node;
	}

	NodeAST* visit(NodeIf &node) override {
		// if the branches are empty, the return path is false
		m_return_path[node.if_body.get()] = !node.if_body->empty();
		node.if_body->accept(*this);

		m_return_path[node.else_body.get()] = !node.else_body->empty();
		node.else_body->accept(*this);
		// if both branches are true, the current block stays true
		const bool if_returns = m_return_path[node.if_body.get()];
		const bool else_returns = m_return_path[node.else_body.get()];
		// m_return_path[m_block_stack.top()] &= if_return and else_return;
		// If BOTH branches guarantee a return, then this entire if-statement
		// guarantees a return for its parent block.
		if (if_returns && else_returns) {
			if (!m_block_stack.empty()) {
				m_return_path[m_block_stack.top()] = true; // Set, do not AND
			}
		}
		return &node;
	}

	NodeAST* visit(NodeSelect &node) override {
		bool all_return = !node.cases.empty();
		for(const auto &cas: node.cases) {
			m_return_path[cas.second.get()] = !cas.second->empty();
			cas.second->accept(*this);
			// all_return &= m_return_path[cas.second.get()];
			// If any case does not return, the whole select statement doesn't guarantee a return.
			if (!m_return_path[cas.second.get()]) {
				all_return = false;
			}
		}
		// m_return_path[m_block_stack.top()] &= all_return;
		if (all_return) {
			if (!m_block_stack.empty()) {
				m_return_path[m_block_stack.top()] = true; // Set, do not AND
			}
		}

		return &node;
	}

	NodeAST* visit(NodeReturn &node) override {
		// m_return_path[m_block_stack.top()] = true;
		// A return statement guarantees a return path for the current block.
		if (!m_block_stack.empty()) {
			m_return_path[m_block_stack.top()] = true;
		}
		return &node;
	}



};