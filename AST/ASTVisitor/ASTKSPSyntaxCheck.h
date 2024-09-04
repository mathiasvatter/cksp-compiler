//
// Created by Mathias Vatter on 18.08.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../SyntaxChecks/KSPPersistency.h"
#include "../../SyntaxChecks/KSPDeclarations.h"

/**
 * Adds persistence functions to variables that are declared with a persistence keyword.
 * Is assigned value to a declaration statement is not constant, split the declaration into a declaration and an assignment statement.
 * CAREFUL: This visitor will mess up the declaration pointers of the references.
 *
 * infos on the max_block_lines:
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
class ASTKSPSyntaxCheck: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
	std::unordered_map<std::string, int> m_ui_control_count;

	static const int MAX_BLOCK_LINES = 4991;
	int max_block_lines = MAX_BLOCK_LINES;
	static const int MAX_UI_CONTROLS = 999;
	static const int MAX_ARRAY_ELEMENTS = 1000000;

	static void check_max_array_size(NodeArray* node) {
		if(node->size->get_node_type() == NodeType::Int) {
			auto node_int = static_cast<NodeInt*>(node->size.get());
			if(node_int->value > MAX_ARRAY_ELEMENTS) {
				auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *node->size);
				error.m_message = "Array size exceeds maximum of " + std::to_string(MAX_ARRAY_ELEMENTS) + ".";
				error.exit();
			}
		}
	}

	void check_max_ui_controls(NodeUIControl* node) {
		if(m_ui_control_count[node->ui_control_type] > MAX_UI_CONTROLS) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *node);
			error.m_message = "Maximum number of UI controls exceeded for <"+node->ui_control_type+">. Counted "+std::to_string(m_ui_control_count[node->ui_control_type])+" controls.";
			error.exit();
		}
	}

	bool check_line_count(NodeBlock* node, int line_count) {
		if(line_count > max_block_lines) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *node);
			error.m_message = "Maximum number of lines in block exceeded ("+std::to_string(max_block_lines)+"). This will probably prompt a 'memory exhausted' error in KSP.";
			error.print();
			return true;
		}
		return false;
	}

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
		return max_block_lines - (block_size/max_block_lines*3)-3;
//		return MAX_BLOCK_LINES - (block_size/MAX_BLOCK_LINES*4)-3;
	}

	std::unique_ptr<NodeBlock> get_block_of_if_stmts(std::vector<std::unique_ptr<NodeBlock>>& blocks) {
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

public:
	explicit ASTKSPSyntaxCheck(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {
		max_block_lines = MAX_BLOCK_LINES;
	};

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);

		static KSPDeclarations declarations;
		auto new_node = node.accept(declarations);
		static KSPPersistency persistency;
		return new_node->accept(persistency);
	}

	NodeAST* visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		if(cast_node<NodeCallback>(node.parent) == m_program->init_callback) {

		}
		node.flatten();
		if(check_line_count(&node, node.statements.size())) {
			auto blocks = split_blocks(node);
			max_block_lines = MAX_BLOCK_LINES;
			return node.replace_with(get_block_of_if_stmts(blocks));
		}
		return &node;
	};

	NodeAST* visit(NodeArray& node) override {
		node.size->accept(*this);
		check_max_array_size(&node);
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		if(node.needs_get_ui_id()) {
			return node.replace_with(std::move(node.wrap_in_get_ui_id()));
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if(node.needs_get_ui_id()) {
			return node.replace_with(std::move(node.wrap_in_get_ui_id()));
		}
		return &node;
	}

	NodeAST* visit(NodeUIControl& node) override {
		m_ui_control_count[node.ui_control_type]++;
		check_max_ui_controls(&node);
		node.control_var->accept(*this);
		node.params->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeIf& node) override {
//		max_block_lines =-1 ;
		node.condition->accept(*this);
		node.if_body->accept(*this);
		node.else_body->accept(*this);
		return &node;
	};

	NodeAST* visit(NodeWhile& node) override {
//		max_block_lines =-1;
		node.condition->accept(*this);
		node.body->accept(*this);
		return &node;
	};


};