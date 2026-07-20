//
// Created by Mathias Vatter on 19.07.26.
//

#pragma once

#include "ASTLowering.h"

/**
 * Lowers optional chaining:
 * pre:
 *	obj?.method(x)
 *  obj?.member := 5
 * post:
 * if obj # nil
 *  obj.method(x) // x will not be evaluated if obj = nil -> short-cuircuiting
 * end if
 * if obj # nil
 *  obj.member := 5
 * end if
 * Multiple ?. in one chain become nested if statements:
 * a?.b?.method() -> if a # nil { if a.b # nil { a.b.method() } }
 * Value contexts (x := obj?.member) have no value when obj is nil and are
 * rejected until nullish coalescing (??) is implemented.
 */
class LoweringOptionalChaining final : public ASTLowering {
public:
	explicit LoweringOptionalChaining(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeAccessChain& node) override {
		if (!node.has_opt_chaining()) {
			return &node;
		}
		// the chain of a ?? expression is consumed by LoweringNullCoalescing
		if (node.parent->cast<NodeNullCoalesce>()) {
			return &node;
		}
		// standalone method call statement:
		// obj?.method(x) -> if(obj # nil) obj.method(x) end if
		if (node.parent->cast<NodeStatement>() and node.chain.back()->cast<NodeFunctionCall>()) {
			const size_t idx = first_opt_index(node);
			const Token curr_opt_chaining = node.opt_chaining_indexes[idx].value();
			auto condition = make_nil_check(node, idx, curr_opt_chaining);
			auto block = std::make_unique<NodeBlock>(node.tok, true);
			auto node_access = std::make_unique<NodeAccessChain>(std::move(node.chain), node.tok);
			node_access->declaration = node.declaration;
			node_access->ty = node.ty;
			// keep the remaining ?. tokens: the recursion below wraps them into nested ifs
			node_access->opt_chaining_indexes = std::move(node.opt_chaining_indexes);
			block->add_as_stmt(std::move(node_access));
			auto node_if = std::make_unique<NodeIf>(
				std::move(condition),
				std::move(block),
				std::make_unique<NodeBlock>(node.tok),
				node.tok
			);
			return node.replace_with(std::move(node_if))->accept(*this);
		}

		// optional chaining used where a value is required
		throw_value_context_error(node).exit();
		return &node;
	}

	NodeAST * visit(NodeSingleAssignment& node) override {
		// an optional chain as r_value has no value to assign when the object is nil
		if (const auto r_access = node.r_value->cast<NodeAccessChain>()) {
			if (r_access->has_opt_chaining()) {
				throw_value_context_error(*r_access).exit();
			}
		}

		// obj?.member := 5 -> if(obj # nil) obj.member := 5 end if
		const auto l_access = node.l_value->cast<NodeAccessChain>();
		if (!l_access or !l_access->has_opt_chaining()) return &node;
		const auto stmt = node.parent->cast<NodeStatement>();
		if (!stmt) return &node;

		const size_t idx = first_opt_index(*l_access);
		const Token curr_opt_chaining = l_access->opt_chaining_indexes[idx].value();
		auto condition = make_nil_check(*l_access, idx, curr_opt_chaining);
		auto block = std::make_unique<NodeBlock>(node.tok, true);
		block->add_as_stmt(std::move(stmt->statement));
		auto node_if = std::make_unique<NodeIf>(
			std::move(condition),
			std::move(block),
			std::make_unique<NodeBlock>(node.tok),
			node.tok
		);
		stmt->set_statement(std::move(node_if));
		// handle the remaining ?. tokens of the l_value with nested if statements
		return stmt->statement->accept(*this);
	}

public:
	static size_t first_opt_index(const NodeAccessChain& node) {
		for (size_t i = 0; i < node.opt_chaining_indexes.size(); i++) {
			if (node.opt_chaining_indexes[i].has_value()) return i;
		}
		return 0;
	}

	/// builds the `<chain prefix> # nil` guard for the ?. at idx and consumes its token.
	/// split resets the token on `node`, so repeated
	/// calls for a chain with multiple ?. (here and in LoweringNullCoalescing) terminate
	/// regardless of what split() does with its own copy of the indexes.
	static std::unique_ptr<NodeBinaryExpr> make_nil_check(NodeAccessChain& node, const size_t idx, const Token& opt_tok) {
		auto first = node.split(idx + 1);
		auto condition = std::make_unique<NodeBinaryExpr>(
			token::NOT_EQUAL,
			std::move(first),
			std::make_unique<NodeNil>(opt_tok),
			opt_tok
		);
		condition->collect_references();
		return condition;
	}

	static Diagnostic throw_value_context_error(const NodeAccessChain& node) {
		const size_t idx = first_opt_index(node);
		const Token tok = node.opt_chaining_indexes[idx].has_value()
			? node.opt_chaining_indexes[idx].value() : node.tok;
		auto error = Diagnostic(ErrorType::SyntaxError, "", "", tok);
		error.message = "Optional chaining cannot be used where a value is required, because there is no value "
			"when the object is <nil>. Provide a fallback with nullish coalescing <obj?.member ?\? fallback>, "
			"or use it as a standalone method call <obj?.method(x)> or assignment target <obj?.member := value>.";
		return error;
	}
};

/**
 * Lowers nullish coalescing into a generated function, mirroring LoweringTernaryOperator,
 * so the expression works in any value context:
 * message(p?.x ?? 0)
 * -->
 * message(opt_chaining0())
 * function opt_chaining0()
 *   if p # nil
 *     return p.x
 *   end if
 *   return 0
 * end function
 * Multiple ?. become nested if statements. Local variables used in the chain or the
 * fallback are promoted to pass-by-reference parameters of the generated function.
 */
class LoweringNullCoalescing final : public ASTLowering {
	std::unordered_set<std::shared_ptr<NodeDataStructure>> m_local_vars;
public:
	explicit LoweringNullCoalescing(NodeProgram *program) : ASTLowering(program) {}

	void set_program(NodeProgram* program) override {
		ASTLowering::set_program(program);
		m_local_vars.clear();
	}

private:

	NodeAST *visit(NodeNullCoalesce &node) override {
		m_local_vars.clear();
		const auto chain = node.chain->cast<NodeAccessChain>();
		if (!chain) return &node; // guaranteed by the parser
		auto type = node.ty;

		// consume every ?. into its nil check, from the outermost to the innermost
		std::vector<std::unique_ptr<NodeBinaryExpr>> conditions;
		while (chain->has_opt_chaining()) {
			const size_t idx = LoweringOptionalChaining::first_opt_index(*chain);
			const Token opt_tok = chain->opt_chaining_indexes[idx].value();
			conditions.push_back(LoweringOptionalChaining::make_nil_check(*chain, idx, opt_tok));
		}

		// innermost statement returns the chain value, nested in one if per ?.
		// (get_definition() is required by later passes, e.g. is_string_env() dereferences
		// it unconditionally for a return statement's direct child -> set below once the
		// owning function definition exists)
		auto chain_return = std::make_unique<NodeReturn>(node.tok, std::move(node.chain));
		const auto chain_return_ptr = chain_return.get();
		std::unique_ptr<NodeAST> guarded = std::move(chain_return);
		for (auto it = conditions.rbegin(); it != conditions.rend(); ++it) {
			auto node_if = std::make_unique<NodeIf>(node.tok);
			node_if->set_condition(std::move(*it));
			node_if->if_body->add_as_stmt(std::move(guarded));
			guarded = std::move(node_if);
		}

		auto body = std::make_unique<NodeBlock>(
			node.tok,
			std::make_unique<NodeStatement>(std::move(guarded), node.tok)
		);
		auto fallback_return = std::make_unique<NodeReturn>(node.tok, std::move(node.fallback));
		const auto fallback_return_ptr = fallback_return.get();
		body->add_as_stmt(std::move(fallback_return));
		body->collect_references();
		body->accept(*this);

		auto func_name = m_program->def_provider->get_fresh_name("opt_chaining");
		auto coalesce_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				node.tok
			),
			node.tok
		);
		coalesce_call->ty = type;

		auto coalesce_def = std::make_shared<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				node.tok
			),
			std::nullopt,
			false,
			std::move(body),
			node.tok
		);
		chain_return_ptr->definition = coalesce_def;
		fallback_return_ptr->definition = coalesce_def;

		// add local vars as pass-by-reference params to the generated function
		auto local_vars = std::move(m_local_vars);
		m_local_vars.clear();
		for (auto& local_var : local_vars) {
			auto param_var = clone_as<NodeDataStructure>(local_var.get());
			param_var->clear_references();

			auto arg = local_var->to_reference();
			coalesce_call->function->add_arg(std::move(arg));

			auto tok = local_var->tok;
			auto param = std::make_unique<NodeFunctionParam>(std::move(param_var), nullptr, tok);
			param->is_pass_by_ref = true;
			coalesce_def->header->add_param(std::move(param));
		}

		coalesce_def->body->scope = true;
		coalesce_def->num_return_stmts = 2;
		coalesce_def->num_return_params = 1;
		coalesce_def->collect_declarations(m_program);
		coalesce_call->kind = NodeFunctionCall::Kind::UserDefined;
		coalesce_call->definition = coalesce_def;
		m_program->add_function_definition(coalesce_def);

		return node.replace_with(std::move(coalesce_call));
	}

	void add_local_var(NodeReference& node) {
		if (node.is_local) {
			if (auto decl = node.get_declaration()) {
				m_local_vars.insert(decl);
				decl->remove_reference(&node);
				node.declaration.reset();
			} else {
				auto error = Diagnostic(ErrorType::VariableError, "Local variable used in nullish coalescing has not been declared: " + node.name, "", node.tok);
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

	NodeAST* visit(NodeAccessChain& node) override {
		return node.accept_locals(*this);
	}

};
