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
	std::unordered_set<std::shared_ptr<NodeDataStructure>> m_local_vars;
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
		if_statement->accept(*this);

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

		// add local vars as params to ternary function
		for (auto& local_var : m_local_vars) {
			auto param_var = clone_as<NodeDataStructure>(local_var.get());
			param_var->clear_references();

			auto arg = local_var->to_reference();
			ternary_call->function->add_arg(std::move(arg));

			auto tok = local_var->tok;
			auto param = std::make_unique<NodeFunctionParam>(std::move(param_var), nullptr, tok);
			param->is_pass_by_ref = true;
			ternary_def->header->add_param(std::move(param));
		}

		ternary_def->body->scope = true;
		ternary_def->num_return_stmts = 2;
		ternary_def->num_return_params = 1;
		ternary_def->collect_declarations(m_program);
		ternary_call->kind = NodeFunctionCall::Kind::UserDefined;
		ternary_call->definition = ternary_def;
		m_program->add_function_definition(ternary_def);

		return node.replace_with(std::move(ternary_call));
	}

	void add_local_var(NodeReference& node) {
		if (node.is_local) {
			if (auto decl = node.get_declaration()) {
				m_local_vars.insert(decl);
				decl->remove_reference(&node);
				node.declaration.reset();
			} else {
				auto error = CompileError(ErrorType::VariableError, "Local variable used in ternary operator has not been declared: " + node.name, "", node.tok);
				error.exit();
			}
		}
	}

	NodeAST * visit(NodePointerRef& node) override {
		add_local_var(node);
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		add_local_var(node);
		return &node;
	}

	NodeAST * visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		add_local_var(node);
		return &node;
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		add_local_var(node);
		return &node;
	}


};