//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * @brief This class is responsible for desugaring for loops into while loops
 * It takes one for loop at a time to rewrite it as a
 * while loop. Nested for loops need to be handled in the
 * parent ASTVisitor class.
 * The transformation is as follows:
 *
 * Pre-desugaring:
 * for o := 0 downto 4-1
 *     message(4+5)
 * end for
 *
 * Post-desugaring:
 * $o := 0
 * while($o >= 3)
 *     message(9)
 *     dec($o)
 * end while
 *
 * IMPORTANT:
 * Also lowers continue statements in for loops to
 * inc(incrementer)
 * continue
 * Otherwise the loop would become infinite!
 *
 */
class LoweringFor final : public ASTLowering {
	std::unique_ptr<NodeAST> m_compound_assignment;
	std::vector<NodeLoop*> m_loop_stack; // to determine whether we are in a while loop of a for loop

public:
	explicit LoweringFor(NodeProgram *program) : ASTLowering(program) {
		m_program = program;
	}

	NodeAST *visit(NodeFor &node) override {
		node.remove_references();
		// function arg
		auto iterator_var = clone_as<NodeReference>(node.iterator->l_value.get());
		auto assign_var = clone_as<NodeReference>(iterator_var.get());
		auto function_var = clone_as<NodeReference>(iterator_var.get());

		if (!node.step) {
			std::unique_ptr<NodeFunctionCall> node_inc;
			// function call
			if (node.to == token::TO) {
				node.step = std::make_unique<NodeInt>(1, node.tok);
			} else {
				node.step = std::make_unique<NodeInt>(-1, node.tok);
			}
		}

		// i += step
		auto compound_assignment = std::make_unique<NodeCompoundAssignment>(
			clone_as<NodeReference>(function_var.get()),
			std::move(node.step),
			token::ADD,
			node.tok
		);

		m_loop_stack.push_back(&node);
		m_compound_assignment = compound_assignment->clone();
		node.body->accept(*this);
		m_compound_assignment = nullptr;
		m_loop_stack.pop_back();

		node.body->add_as_stmt(std::move(compound_assignment));
		node.body->get_last_statement()->desugar(m_program);

		// handle while condition
		auto comparison_op = token::LESS_EQUAL;
		if (node.to == token::DOWNTO) comparison_op = token::GREATER_EQUAL;
		// make comparison expression
		auto comparison = std::make_unique<NodeBinaryExpr>(
			comparison_op,
			std::move(iterator_var),
			std::move(node.iterator_end),
			node.tok
		);
		comparison->ty = TypeRegistry::Comparison;


		auto node_while_statement = std::make_unique<NodeWhile>(
			std::move(comparison),
			std::move(node.body),
			node.tok
		);

		auto node_assign_statement = std::make_unique<NodeSingleAssignment>(
			std::move(assign_var),
			std::move(node.iterator->r_value),
			node.tok
		);

		auto node_body = std::make_unique<NodeBlock>(node.tok);
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assign_statement), node.tok));
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_while_statement), node.tok));
		node_body->collect_references();
		return node.replace_with(std::move(node_body));
	}

	// inside while loops no lowering of continue statements
	NodeAST * visit(NodeWhile& node) override {
		m_loop_stack.push_back(&node);
		ASTVisitor::visit(node);
		m_loop_stack.pop_back();
		return &node;
	}

	NodeAST * visit(NodeForEach& node) override {
		m_loop_stack.push_back(&node);
		ASTVisitor::visit(node);
		m_loop_stack.pop_back();
		return &node;
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);

		if (node.kind != NodeFunctionCall::Kind::Builtin) return &node;
		if (m_loop_stack.back() != m_loop_stack.front()) return &node;
		if(node.function->name == "continue" and m_compound_assignment) {
			auto block = std::make_unique<NodeBlock>(node.tok, false);
			block->add_as_stmt(m_compound_assignment->clone());
			block->get_last_statement()->desugar(m_program);
			block->add_as_stmt(DefinitionProvider::continu(node.tok));
			return node.replace_with(std::move(block));
		}
		return &node;
	}

};
