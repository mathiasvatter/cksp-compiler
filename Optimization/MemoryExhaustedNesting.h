//
// Created by Mathias Vatter on 05.09.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"

/**
 * @class MemoryExhaustedNesting
 * @brief Optimizes nested blocks in an AST to prevent memory exhaustion errors.
 *
 * This class handles large blocks of code by breaking them into smaller blocks and using nested if-statements.
 * It defines several constants representing token limits for different types of statements.
 */
class MemoryExhaustedNesting : public ASTOptimizations {
private:
    static const int ONE_TOKEN_LIMIT = 4993; ///< Token limit for declarations and functions with no params.
	static const int TWO_TOKEN_LIMIT = 4991; ///< Token limit for assignments and functions with one param.
	static const int THREE_TOKEN_LIMIT = 4989; ///< Token limit for functions with two params.
	static const int FOUR_TOKEN_LIMIT = 4987; ///< Token limit for UI controls.
	static const int FIVE_TOKEN_LIMIT = 4985; ///< Token limit for if-statements with empty else blocks and while statements.
	static const int SIX_TOKEN_LIMIT = 4984; ///< Token limit for if-statements with non-empty else blocks.
	static const int SEVEN_TOKEN_LIMIT = 4981; ///< Token limit for other cases.
	static const int NKS_TOKEN_LIMIT = 2495; ///< Token limit for NKS2 functions.
	static inline const std::vector<int> TOKEN_LIMITS = {ONE_TOKEN_LIMIT, TWO_TOKEN_LIMIT, THREE_TOKEN_LIMIT, FOUR_TOKEN_LIMIT, FIVE_TOKEN_LIMIT, SIX_TOKEN_LIMIT, SEVEN_TOKEN_LIMIT, NKS_TOKEN_LIMIT};

	/**
     * @brief Calculates points based on the token limit.
     * @param token_limit The token limit.
     * @return The calculated points.
     */
	static double points(int token_limit) {
		return ONE_TOKEN_LIMIT/static_cast<double>(token_limit);
	}

	/**
     * @brief Gets points based on the number of tokens.
     * @param tokens The number of tokens.
     * @return The calculated points.
     */
	static double get_points(int tokens) {
		if(tokens < TOKEN_LIMITS.size())
			return points(TOKEN_LIMITS[tokens]);
		return points(NKS_TOKEN_LIMIT);
	}

	int m_line_limit = 0;

public:
	explicit MemoryExhaustedNesting() {
		m_line_limit = 0;
	};

	/**
     * @brief Visits a NodeBlock and optimizes it to prevent memory exhaustion.
     * @param node The NodeBlock to visit.
     * @return The optimized NodeAST.
     */
	NodeAST* visit(NodeBlock& node) override {
		std::vector<std::pair<int, int>> idx_ranges;
		idx_ranges.emplace_back(-1,-1);
		double block_points = 0;
		for(int i=0; i<node.statements.size(); i++) {
			auto& stmt = node.statements[i];
			stmt->accept(*this);
			block_points += get_points(stmt->get_bison_tokens());
			if(check_line_count(&node, block_points)) {
				idx_ranges.emplace_back(idx_ranges.back().second+1, i-1);
				block_points = get_points(stmt->get_bison_tokens());
			}
		}
		// add last range if not already added
		if(idx_ranges.back().second != node.statements.size()-1) {
			idx_ranges.emplace_back(idx_ranges.back().second+1, node.statements.size()-1);
		}
		// remove first element (placeholder)
		idx_ranges.erase(idx_ranges.begin());
		if(idx_ranges.size() > 1) {
			auto blocks = apply_ranges_to_block(node, idx_ranges);
			auto new_block = node.replace_with(get_block_of_if_stmts(blocks));
			// check if new block of if-statements is too large -> accept again then
			if(static_cast<int>(blocks.size() * points(FIVE_TOKEN_LIMIT)) > ONE_TOKEN_LIMIT) {
				new_block->accept(*this);
			}
			return new_block;
		}
		return &node;
	};

private:
	/**
     * @brief Checks if the block exceeds the line limit.
     * @param node The NodeBlock to check.
     * @param block_points The accumulated points for the block.
     * @return True if the block exceeds the line limit, false otherwise.
     */
	static bool check_line_count(NodeBlock* node, double block_points) {
		if(static_cast<int>(block_points) >= ONE_TOKEN_LIMIT) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *node);
			error.m_message = "Fixed possible 'memory exhausted' error by applying nested <if-statements> 'if(1=1)'. Consider using "
							  "<Arrays> or loading separate *.nka files for static initializations to reduce the number of lines.";
			error.print();
			return true;
		}
		return false;
	}

	/**
     * @brief Applies index ranges to a block, creating new blocks for each range.
     * @param block The original NodeBlock.
     * @param idx_ranges The index ranges to apply.
     * @return A vector of new NodeBlocks.
     */
	static std::vector<std::unique_ptr<NodeBlock>> apply_ranges_to_block(NodeBlock& block, const std::vector<std::pair<int, int>>& idx_ranges) {
		std::vector<std::unique_ptr<NodeBlock>> result_blocks;

		for (const auto& range : idx_ranges) {
			auto new_block = std::make_unique<NodeBlock>(block.tok);
			for (int i = range.first; i <= range.second; ++i) {
				new_block->add_stmt(std::move(block.statements[i]));
			}
			result_blocks.push_back(std::move(new_block));
		}

		block.statements.clear(); // empty original block
		return result_blocks;
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
			new_block->add_stmt(std::make_unique<NodeStatement>(std::move(node_if), tok));
		}
		return new_block;
	}




};


