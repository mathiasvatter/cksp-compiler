//
// Created by Mathias Vatter on 05.09.24.
//

#pragma once

#include "../ASTVisitor/ASTOptimizations.h"

/**
 * @class MemoryExhaustedNesting
 * @brief Keeps the generated script within the parser stack limit of Kontakt's KSP parser.
 *
 * Kontakt parses KSP with a bison parser whose stack is capped at YYMAXDEPTH = 5000 entries. Exceeding it
 * aborts loading with "memory exhausted". Statement lists and the list of callbacks and functions are
 * right-recursive, so every completed statement keeps exactly one stack entry until its enclosing list ends.
 * The depth while parsing a statement is therefore
 *   the number of earlier callbacks/functions
 * + the earlier statements in every enclosing statement list
 * + a fixed offset for every enclosing if/while/select
 * + a small peak for the statement itself (deeper for nested expressions).
 * Token counts do not matter.
 *
 * A statement list that would exceed the budget is split into chunks wrapped in `if(1=1)`. A finished `if`
 * collapses to one stack entry, so a wrapped chunk costs the list it sits in only one slot.
 *
 * The constants were measured against the parse tables of Kontakt 7. Kontakt 8 uses the same YYMAXDEPTH.
 */
class MemoryExhaustedNesting : public ASTOptimizations {
	/// Highest parser stack index Kontakt accepts: the stack of 5000 entries is full at index 4999.
	static constexpr int KSP_MAX_DEPTH = 4998;
	/// Headroom for statement shapes the peak estimate does not cover exactly.
	static constexpr int SAFETY_MARGIN = 32;
	static constexpr int DEPTH_BUDGET = KSP_MAX_DEPTH - SAFETY_MARGIN;
	/// Depth of the first statement of a callback or function, on top of the number of earlier ones.
	static constexpr int CALLBACK_BODY_OFFSET = 2;
	/// Depth of the first body statement, relative to the depth of the line that opens the construct.
	static constexpr int IF_BODY_OFFSET = 5;
	static constexpr int ELSE_BODY_OFFSET = 7;
	static constexpr int WHILE_BODY_OFFSET = 5;
	static constexpr int CASE_BODY_OFFSET = 10;
	/// How far a flat statement pushes the stack above its own slot (measured maximum: 14).
	static constexpr int STATEMENT_PEAK = 16;
	/// Additional depth per nested expression operand (measured maximum: 4 for a parenthesised right operand).
	static constexpr int NESTED_OPERAND_DEPTH = 4;
	/// Left operands that are themselves parenthesised binary expressions open one paren each.
	static constexpr int NESTED_LEFT_OPERAND_DEPTH = 1;

	/// Upper bound for the parser depth an expression adds while it is being parsed.
	class ExpressionDepth final : public ASTVisitor {
		int m_depth = 0;
		int m_max_depth = 0;

		void enter(NodeAST& child, const int depth) {
			m_depth += depth;
			m_max_depth = std::max(m_max_depth, m_depth);
			child.accept(*this);
			m_depth -= depth;
		}

	public:
		int get(NodeAST& node) {
			m_depth = 0;
			m_max_depth = 0;
			node.accept(*this);
			return m_max_depth;
		}

		NodeAST* visit(NodeBinaryExpr& node) override {
			// mirrors ASTGenerator: nested binary expressions are parenthesised unless they are strings
			const bool parenthesised_left = node.left->cast<NodeBinaryExpr>() and node.left->ty != TypeRegistry::String;
			enter(*node.left, parenthesised_left ? NESTED_LEFT_OPERAND_DEPTH : 0);
			enter(*node.right, NESTED_OPERAND_DEPTH);
			return &node;
		}

		NodeAST* visit(NodeUnaryExpr& node) override {
			enter(*node.operand, NESTED_OPERAND_DEPTH);
			return &node;
		}

		NodeAST* visit(NodeFunctionCall& node) override {
			enter(*node.function, NESTED_OPERAND_DEPTH);
			return &node;
		}

		NodeAST* visit(NodeArrayRef& node) override {
			if (node.index) enter(*node.index, NESTED_OPERAND_DEPTH);
			return &node;
		}
	};

	ExpressionDepth m_expression_depth;

public:
	/**
	 * @brief Visits the callbacks and functions in the order ASTGenerator prints them.
	 */
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		int top_level_index = 0;
		const auto fix_top_level = [&](NodeBlock& body) {
			fix_block(body, top_level_index++ + CALLBACK_BODY_OFFSET);
		};
		if (!node.callbacks.empty()) fix_top_level(*node.callbacks[0]->statements);
		for (const auto& function : node.function_definitions) {
			fix_top_level(*function->body);
		}
		for (size_t i = 1; i < node.callbacks.size(); i++) {
			fix_top_level(*node.callbacks[i]->statements);
		}
		return &node;
	}

private:
	/**
	 * @brief Makes sure no statement in @p block exceeds the budget when its first statement sits at @p base.
	 */
	void fix_block(NodeBlock& block, const int base) {
		if (base + demand(block) <= DEPTH_BUDGET) return;

		// the generator prints nested blocks inline anyway; flat statements occupy exactly one slot each
		block.flatten(true);
		std::erase_if(block.statements, [](const std::unique_ptr<NodeStatement>& stmt) {
			return stmt->statement->cast<NodeDeadCode>() != nullptr;
		});

		const int available = DEPTH_BUDGET - base;
		const int chunk_limit = (available - IF_BODY_OFFSET) / 2;
		if (chunk_limit <= STATEMENT_PEAK) {
			report_unfixable(block);
			return;
		}
		if (static_cast<int>(block.statements.size()) > chunk_limit) {
			split_into_chunks(block, base, chunk_limit);
		} else {
			fix_nested_blocks(block, base);
		}
		if (base + demand(block) > DEPTH_BUDGET) {
			report_unfixable(block);
		}
	}

	/**
	 * @brief Wraps the statements of @p block into chunks of `if(1=1)` so that each chunk stays within
	 * @p chunk_limit, then fixes every chunk at the depth it ends up at.
	 */
	void split_into_chunks(NodeBlock& block, const int base, const int chunk_limit) {
		std::vector<std::unique_ptr<NodeBlock>> chunks;
		int chunk_index = 0;
		for (auto& stmt : block.statements) {
			const int need = statement_demand(*stmt->statement);
			if (chunks.empty() or (chunk_index > 0 and chunk_index + need > chunk_limit)) {
				chunks.push_back(std::make_unique<NodeBlock>(block.tok));
				chunk_index = 0;
			}
			chunks.back()->add_stmt(std::move(stmt));
			chunk_index++;
		}
		block.statements.clear();
		// one more level of wrapping would not fit next to full chunks; this needs millions of statements
		if (static_cast<int>(chunks.size()) > chunk_limit) {
			for (auto& chunk : chunks) {
				for (auto& stmt : chunk->statements) block.add_stmt(std::move(stmt));
			}
			report_unfixable(block);
			return;
		}

		auto error = ASTVisitor::make_diagnostic(ErrorType::SyntaxError, block);
		error.message = "Fixed possible 'memory exhausted' error by applying nested <if-statements> 'if(1=1)'. Consider using "
						  "<Arrays> or loading separate *.nka files for static initializations to reduce the number of lines.";
		error.report(diagnostics());

		auto wrapped = get_block_of_if_stmts(chunks);
		for (auto& stmt : wrapped->statements) block.add_stmt(std::move(stmt));
		for (int i = 0; i < static_cast<int>(block.statements.size()); i++) {
			const auto node_if = block.statements[i]->statement->cast<NodeIf>();
			fix_block(*node_if->if_body, base + i + IF_BODY_OFFSET);
		}
	}

	/**
	 * @brief Fixes the bodies of the if/while/select statements in @p block at the depth they start at.
	 */
	void fix_nested_blocks(NodeBlock& block, const int base) {
		for (int i = 0; i < static_cast<int>(block.statements.size()); i++) {
			const int depth = base + i;
			auto& stmt = *block.statements[i]->statement;
			if (const auto node_if = stmt.cast<NodeIf>()) {
				fix_block(*node_if->if_body, depth + IF_BODY_OFFSET);
				fix_block(*node_if->else_body, depth + ELSE_BODY_OFFSET);
			} else if (const auto node_while = stmt.cast<NodeWhile>()) {
				fix_block(*node_while->body, depth + WHILE_BODY_OFFSET);
			} else if (const auto node_select = stmt.cast<NodeSelect>()) {
				for (auto& [labels, body] : node_select->cases) {
					fix_block(*body, depth + CASE_BODY_OFFSET);
				}
			}
		}
	}

	void report_unfixable(const NodeBlock& block) {
		auto error = ASTVisitor::make_diagnostic(ErrorType::SyntaxError, block);
		error.message = "This block is likely to cause a 'memory exhausted' error in Kontakt and could not be fixed by applying "
						  "nested <if-statements> 'if(1=1)'. Reduce the number of statements or the nesting depth around it.";
		error.report(diagnostics());
	}

	/**
	 * @brief Highest parser depth reached in @p block, relative to the depth of its first statement.
	 */
	int demand(NodeBlock& block) {
		int index = 0;
		int max_depth = 0;
		accumulate_demand(block, index, max_depth);
		return max_depth;
	}

	void accumulate_demand(NodeBlock& block, int& index, int& max_depth) {
		for (const auto& stmt : block.statements) {
			auto& node = *stmt->statement;
			if (node.cast<NodeDeadCode>()) continue;
			if (const auto inner = node.cast<NodeBlock>()) {
				// printed inline by the generator, followed by an empty line that counts as a statement
				accumulate_demand(*inner, index, max_depth);
			} else {
				max_depth = std::max(max_depth, index + statement_demand(node));
			}
			index++;
		}
	}

	/**
	 * @brief Highest parser depth reached while parsing @p node, relative to the slot it occupies.
	 */
	int statement_demand(NodeAST& node) {
		if (const auto node_if = node.cast<NodeIf>()) {
			int need = std::max(expression_demand(*node_if->condition), IF_BODY_OFFSET + demand(*node_if->if_body));
			if (!node_if->else_body->statements.empty()) {
				need = std::max(need, ELSE_BODY_OFFSET + demand(*node_if->else_body));
			}
			return need;
		}
		if (const auto node_while = node.cast<NodeWhile>()) {
			return std::max(expression_demand(*node_while->condition), WHILE_BODY_OFFSET + demand(*node_while->body));
		}
		if (const auto node_select = node.cast<NodeSelect>()) {
			int need = expression_demand(*node_select->expression);
			for (auto& [labels, body] : node_select->cases) {
				need = std::max(need, CASE_BODY_OFFSET + demand(*body));
			}
			return need;
		}
		if (const auto inner = node.cast<NodeBlock>()) {
			return demand(*inner) + 1;
		}
		return expression_demand(node);
	}

	int expression_demand(NodeAST& node) {
		return STATEMENT_PEAK + m_expression_depth.get(node);
	}

	static std::unique_ptr<NodeBlock> get_block_of_if_stmts(std::vector<std::unique_ptr<NodeBlock>>& blocks) {
		Token tok = blocks[0]->tok;
		auto new_block = std::make_unique<NodeBlock>(tok);
		auto true_expr = std::make_unique<NodeBinaryExpr>(token::EQUAL, std::make_unique<NodeInt>(1,tok), std::make_unique<NodeInt>(1,tok),tok);
		true_expr->ty = TypeRegistry::Boolean;
		for(auto& block : blocks) {
			auto node_if = std::make_unique<NodeIf>(
				true_expr->clone(),
				std::move(block),
				std::make_unique<NodeBlock>(tok),
				tok
			);
			new_block->add_as_stmt(std::move(node_if));
		}
		return new_block;
	}

};
