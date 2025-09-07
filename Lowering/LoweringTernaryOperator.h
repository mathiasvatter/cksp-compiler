//
// Created by Mathias Vatter on 07.09.25.
//

#pragma once

#include "ASTLowering.h"

/**
 * This class lowers ternary operators into if statements or inline integer expressions using <LoweringBooleanExpression>
 * For example:
 * message( 1 < 2 ? 1 : 2)
 * -->
 * message(CKSP::__lt__(1, 2) * 1 + (1-CKSP::__lt__(1, 2)) * 2)
 * or in case of strings or function calls in the branches using a temp var:
 * message(1 < 2 ? "hello world" : "Goodbye world")
 * -->
 * declare local tmp
 * if 1 < 2
 * 	tmp := "hello world"
 * else
 * 	tmp := "Goodbye world"
 * end if
 * message(tmp)
 *
 */
class LoweringTernaryOperator final : public ASTLowering {
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeBlock>>> m_blocks_per_stmt;
	NodeStatement* m_current_statement = nullptr;
public:
	explicit LoweringTernaryOperator(NodeProgram *program) : ASTLowering(program) {}

	NodeAST *lower_ternaries(NodeProgram &node) {
		m_program = &node;
		m_blocks_per_stmt.clear();
		m_current_statement = nullptr;
		node.accept(*this);
		insert_blocks_in_statements();
		m_program->reset_function_visited_flag();
		return &node;
	}


private:
	void insert_blocks_in_statements() {
		for (auto & [stmt, blocks] : m_blocks_per_stmt) {
			if (blocks.empty()) continue;
			for (int i = 0; i < (int)blocks.size(); i++) {
				auto& block = blocks[i];
				block->add_as_stmt(std::move(stmt->statement));
				stmt->set_statement(std::move(block));
			}
		}
		m_blocks_per_stmt.clear();
	}

	NodeAST *visit(NodeStatement &node) override {
		m_current_statement = &node;
		node.statement->accept(*this);
		m_current_statement = nullptr;
		return &node;
	}

	NodeAST *visit(NodeFunctionCall &node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		auto definition = node.get_definition();
		if(definition and node.kind != NodeFunctionCall::Kind::Builtin) {
			if(!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;
		}

		return &node;
	}

	NodeAST *visit(NodeTernary &node) override {
		auto pre_ternary_stmt = m_current_statement;
		auto tmp_var = m_program->get_tmp_var(node.if_branch->ty);
		auto tmp_decl = std::make_unique<NodeSingleDeclaration>(
			tmp_var,
			nullptr,
			node.tok
		);
		auto tmp_ref_to_replace = tmp_var->to_reference();
		auto if_assignment = std::make_unique<NodeSingleAssignment>(
			tmp_decl->variable->to_reference(),
			std::move(node.if_branch),
			node.tok
		);
		auto else_assignment = std::make_unique<NodeSingleAssignment>(
			tmp_decl->variable->to_reference(),
			std::move(node.else_branch),
			node.tok
		);

		auto if_statement = std::make_unique<NodeIf>(node.tok);
		if_statement->set_condition(std::move(node.condition));
		if_statement->if_body->add_as_stmt(std::move(if_assignment));
		if_statement->else_body->add_as_stmt(std::move(else_assignment));

		// get nested ternary expressions
		if_statement->accept(*this);

		auto block = std::make_unique<NodeBlock>(node.tok, true);
		block->add_as_stmt(std::move(tmp_decl));
		block->add_as_stmt(std::move(if_statement));
		m_blocks_per_stmt[pre_ternary_stmt].push_back(std::move(block));


		return node.replace_with(std::move(tmp_ref_to_replace));
	}


};