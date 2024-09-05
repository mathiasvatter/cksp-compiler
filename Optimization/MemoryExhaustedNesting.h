//
// Created by Mathias Vatter on 05.09.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"

/**
 * infos on the m_line_limit:
 * // one param functions
 * define ASSIGNMENT_DECLARATION := 4992
 * define ZERO_PARAM_FUNC := 4994
 * define ONE_PARAM_FUNC := 4991
 * define TWO_PARAM_FUNC := 4989
 * define THREE_PARAM_FUNC := 4987
 * define FOUR_PARAM_FUNC := 4985
 * define FIVE_PARAM_FUNC := 4985
 * define NKS2_FUNC := 2495
 *
 * define ONE_TOKEN := 4993 // declarations, functions with no params
 * define TWO_TOKEN := 4991 // assigments, functions with one param
 * define THREE_TOKEN := 4989 // functions with two params
 * define FOUR_TOKEN := 4987
 * define FIVE_TOKEN := 4985 // if-statements with empty else statements and while statements
 * define SIX_TOKEN := 4984 // if statements with non-empty else blocks
 *
 * define pts_one_token := ONE_TOKEN/ONE_TOKEN 1
 * define pts_two_token := TWO_TOKEN/ONE_TOKEN 4993/4991
 * define pts_three_token := THREE_TOKEN/ONE_TOKEN 4993/4989
 * define pts_four_token := FOUR_TOKEN/ONE_TOKEN 4993/4987
 * define pts_five_token := FIVE_TOKEN/ONE_TOKEN 4993/4985
 * define pts_six_token := SIX_TOKEN/ONE_TOKEN 4993/4984
 * define pts_nks_token := ONE_TOKEN/NKS2_FUNC 4993/2495
 */


class MemoryExhaustedNesting : public ASTOptimizations {
private:
//	static const int MAX_BLOCK_LINES = 4991;
	static const int ONE_TOKEN_LIMIT = 4993; // declarations, functions with no params
	static const int TWO_TOKEN_LIMIT = 4991; // assigments, functions with one param
	static const int THREE_TOKEN_LIMIT = 4989; // functions with two params
	static const int FOUR_TOKEN_LIMIT = 4987; // ui_controls?
	static const int FIVE_TOKEN_LIMIT = 4985; // if-statements with empty else statements and while statements
	static const int SIX_TOKEN_LIMIT = 4984; // if statements with non-empty else blocks
	static const int SEVEN_TOKEN_LIMIT = 4981;
	static const int NKS_TOKEN_LIMIT = 2495;
	static inline const std::vector<int> TOKEN_LIMITS = {ONE_TOKEN_LIMIT, TWO_TOKEN_LIMIT, THREE_TOKEN_LIMIT, FOUR_TOKEN_LIMIT, FIVE_TOKEN_LIMIT, SIX_TOKEN_LIMIT, SEVEN_TOKEN_LIMIT, NKS_TOKEN_LIMIT};
	static inline double points(int token_limit) {
		return ONE_TOKEN_LIMIT/static_cast<double>(token_limit);
	}

	static inline double get_points(int tokens) {
		if(tokens < TOKEN_LIMITS.size())
			return points(TOKEN_LIMITS[tokens]);
		return points(NKS_TOKEN_LIMIT);
	}

	int m_line_limit = 0;

public:
	explicit MemoryExhaustedNesting() {
		m_line_limit = 0;
	};

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
				block_points = get_points_per_stmt(*stmt->statement);
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

	static bool check_line_count(NodeBlock* node, double block_points) {
		if(static_cast<int>(block_points) >= ONE_TOKEN_LIMIT) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *node);
			error.m_message = "Fixed 'memory exhausted' error by applying nested <if-statements> 'if(1=1)'. Consider using "
							  "<Arrays> or loading separate *.nka files for static initializations to reduce the number of lines.";
			error.print();
			return true;
		}
		return false;
	}

	static std::vector<std::unique_ptr<NodeBlock>> apply_ranges_to_block(NodeBlock& block, const std::vector<std::pair<int, int>>& idx_ranges) {
		std::vector<std::unique_ptr<NodeBlock>> result_blocks;

		// Iteriere über die Bereiche in idx_ranges
		for (const auto& range : idx_ranges) {
			auto new_block = std::make_unique<NodeBlock>(block.tok); // Erstelle einen neuen Block

			// bewege die Statements aus dem Bereich [range.first, range.second] in den neuen Block
			for (int i = range.first; i <= range.second; ++i) {
				new_block->add_stmt(std::move(block.statements[i]));
			}

			result_blocks.push_back(std::move(new_block)); // Füge den neuen Block der Ergebnisliste hinzu
		}

		block.statements.clear(); // Leere den ursprünglichen Block, wenn du möchtest
		return result_blocks;
	}

	// splits a block into multiple blocks with a maximum of MAX_BLOCK_LINES
	std::vector<std::unique_ptr<NodeBlock>> split_blocks(NodeBlock &block) {
		int new_max_block_lines = calc_adjusted_max_block_lines(block);
		std::vector<std::unique_ptr<NodeBlock>> result_blocks;
		auto it = block.statements.begin();
		while (it != block.statements.end()) {
			// Erstelle einen neuen Block mit vorreserviertem Speicherplatz
			auto new_block = std::make_unique<NodeBlock>(block.tok);
			new_block->statements.reserve(new_max_block_lines);

			// Kopiere bis zu max_block_size Statements in den neuen Block
			for (size_t i = 0; i < new_max_block_lines && it != block.statements.end(); ++i, ++it) {
				new_block->add_stmt(std::move((*it)));
			}

			// Füge den neuen Block zum Ergebnis hinzu
			result_blocks.push_back(std::move(new_block));
		}
		block.statements.clear();
		return result_blocks;
	}

	/// calculate new max block size, since for every control flow statement created, the max block size inside them
	/// shrinks by 3 stmts
	inline int calc_adjusted_max_block_lines(NodeBlock& block) const {
		int block_size = block.statements.size();
		return m_line_limit - (block_size/m_line_limit*3)-3;
//		return MAX_BLOCK_LINES - (block_size/MAX_BLOCK_LINES*4)-3;
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

	static inline double get_points_per_stmt(NodeAST& node) {
		int num_tokens = 0;
		switch(node.node_type) {
			case NodeType::SingleDeclaration: {
				num_tokens = 1;
				// if ui_control -> three tokens
				auto single_decl = static_cast<NodeSingleDeclaration *>(&node);
				if(single_decl->value) {
					num_tokens += 1;
				}
				if(single_decl->variable->get_node_type() == NodeType::UIControl) {
					auto ui_control = static_cast<NodeUIControl*>(single_decl->variable.get());
					num_tokens += 1; // because of ui_button keyword e.g.
					num_tokens += ui_control->params->params.size();
				}
				if(single_decl->variable->get_node_type() == NodeType::Array) {
					num_tokens += 1; // because of [] brackets
				}
				break;
			}
			case NodeType::SingleAssignment: {
				num_tokens = 2;
				auto node_assign = static_cast<NodeSingleAssignment *>(&node);
				if(node_assign->l_value->get_node_type() == NodeType::ArrayRef) {
					num_tokens += 1; // because of [] brackets
				}
				break;
			}
			case NodeType::FunctionCall: {
				num_tokens = 1; // because of function name
				auto func_call = static_cast<NodeFunctionCall *>(&node);
				// called functions have a one token limit
				if (func_call->kind != NodeFunctionCall::Kind::Builtin) {
					break;
				}
				// check if it is nks2 function
				if(contains(func_call->function->name, "nks")) {
					return points(NKS_TOKEN_LIMIT);
				}
				num_tokens += func_call->function->args->params.size();
				break;
			}
			case NodeType::If: {
				num_tokens = 5;
				auto node_if = static_cast<NodeIf *>(&node);
				if(!node_if->else_body->statements.empty()) {
					num_tokens += 1;
				}
				break;
			}
			case NodeType::While:
				num_tokens = 5;
				break;
			default:
				num_tokens = 1;
				break;
		}
		return get_points(num_tokens);
	}


};


