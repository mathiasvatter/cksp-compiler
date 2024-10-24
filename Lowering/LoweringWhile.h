//
// Created by Mathias Vatter on 23.10.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * Lowers break statements inside loops by adding a flag to the loop and a continue statement afterwards.
 *
 * Example:
 * i := 0
 * while (i < 5) {
 *     inc(i);
 *     if (i <= 3) {
 *         break; // break out of the loop
 *     }
 *     message(i);
 * }
 *
 * becomes:
 *
 * declare EXIT_FLAG // generate local exit variable with fresh name
 * i := 0;
 * while (i < 5 && EXIT_FLAG = 0) {
 *     inc(i);
 *     if (i <= 3) {
 *         EXIT_FLAG := 1;
 *         continue;
 *     }
 *     message(i);
 * }
 */
class LoweringWhile : public ASTLowering {
private:
	bool m_has_break = false;
	std::string m_exit_flag_name;
public:
	explicit LoweringWhile(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeWhile& node) override {
		m_has_break = false;
		m_exit_flag_name = m_program->def_provider->get_fresh_name("EXIT_FLAG");
//		node.condition->accept(*this);
		node.body->accept(*this);

		if(!m_has_break) return &node;

		// add exit flag to condition
		auto exit_flag_var = std::make_unique<NodeVariable>(
			std::nullopt,
			m_exit_flag_name,
			TypeRegistry::Integer,
			DataType::Mutable,
			node.tok
		);
		exit_flag_var->is_local = true;
		auto exit_flag_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(exit_flag_var),
			nullptr,
			node.tok
		);
		auto exit_flag_ref = exit_flag_decl->variable->to_reference();
		exit_flag_ref->match_data_structure(exit_flag_decl->variable.get());
		node.condition = std::make_unique<NodeBinaryExpr>(
			token::BOOL_AND,
			std::move(node.condition),
			std::make_unique<NodeBinaryExpr>(
				token::EQUAL,
				std::move(exit_flag_ref),
				std::make_unique<NodeInt>(0, node.tok),
				node.tok
			),
			node.tok
		);
		node.condition->parent = &node;

		auto block = std::make_unique<NodeBlock>(node.tok);
		block->add_stmt(std::make_unique<NodeStatement>(std::move(exit_flag_decl), node.tok));
		block->add_stmt(std::make_unique<NodeStatement>(
			std::make_unique<NodeWhile>(
				std::move(node.condition),
				std::move(node.body),
				node.tok
			),
			node.tok
			)
		);
		return node.replace_with(std::move(block));
	}

	NodeAST * visit(NodeBreak& node) override {
		m_has_break = true;
		auto exit_flag_assign = std::make_unique<NodeSingleAssignment>(
			std::make_unique<NodeVariableRef>(m_exit_flag_name, node.tok),
			std::make_unique<NodeInt>(1, node.tok),
			node.tok
		);
		auto continue_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionVarRef>(
				std::make_unique<NodeFunctionHeader>(
					"continue",
					std::make_unique<NodeParamList>(
						node.tok
					),
					node.tok
				),
				node.tok
			),
			node.tok
		);
		continue_call->function->header->has_forced_parenth = false;
		auto block = std::make_unique<NodeBlock>(node.tok);
		block->add_stmt(std::make_unique<NodeStatement>(std::move(exit_flag_assign), node.tok));
		block->add_stmt(std::make_unique<NodeStatement>(std::move(continue_call), node.tok));
		return node.replace_with(std::move(block));
	}

};