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
 * or in case of strings or function calls in the branches using ternary() function calls
 * message(1 < 2 ? "hello world" : "Goodbye world")
 * -->
 * message(ternary())
 * function ternary()
 *   if 1 < 2
 *   	return "hello world"
 *   else
 *   	return "Goodbye world"
 *   end if
 * end function
 *
 */
class LoweringTernaryOperator final : public ASTLowering {
public:
	explicit LoweringTernaryOperator(NodeProgram *program) : ASTLowering(program) {}

private:

	NodeAST *visit(NodeTernary &node) override {
		auto type = node.if_branch->ty; // should be the same as else_branch->ty
		auto if_return = std::make_unique<NodeReturn>(
			node.tok,
			std::move(node.if_branch)
		);
		auto else_return = std::make_unique<NodeReturn>(
			node.tok,
			std::move(node.else_branch)
		);

		auto if_statement = std::make_unique<NodeIf>(node.tok);
		if_statement->set_condition(std::move(node.condition));
		if_statement->if_body->add_as_stmt(std::move(if_return));
		if_statement->else_body->add_as_stmt(std::move(else_return));
		if_statement->collect_references();
		// get nested ternary expressions
		// if_statement->accept(*this);

		auto func_name = m_program->def_provider->get_fresh_name("ternary");
		auto ternary_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				node.tok
			),
			node.tok
		);
		ternary_call->ty = type;

		auto ternary_def = std::make_shared<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				node.tok
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(
				node.tok,
				std::make_unique<NodeStatement>(std::move(if_statement), node.tok)
			),
			node.tok
		);
		ternary_def->body->scope = true;
		ternary_def->num_return_stmts = 2;
		ternary_def->num_return_params = 1;
		ternary_call->kind = NodeFunctionCall::Kind::UserDefined;
		ternary_call->definition = ternary_def;
		m_program->add_function_definition(ternary_def);

		return node.replace_with(std::move(ternary_call));
	}


};