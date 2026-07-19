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

private:
	static size_t first_opt_index(const NodeAccessChain& node) {
		for (size_t i = 0; i < node.opt_chaining_indexes.size(); i++) {
			if (node.opt_chaining_indexes[i].has_value()) return i;
		}
		return 0;
	}

	/// builds the `<chain prefix> # nil` guard for the ?. at idx and consumes its token
	static std::unique_ptr<NodeBinaryExpr> make_nil_check(NodeAccessChain& node, const size_t idx, const Token& opt_tok) {
		auto first = node.split(idx + 1);
		// the check itself guards this ?. -> the split-off prefix carries no optional token
		if (const auto first_chain = cast_node<NodeAccessChain>(first.get())) {
			first_chain->opt_chaining_indexes[idx].reset();
		}
		return std::make_unique<NodeBinaryExpr>(
			token::NOT_EQUAL,
			std::move(first),
			std::make_unique<NodeNil>(opt_tok),
			opt_tok
		);
	}

	static Diagnostic throw_value_context_error(const NodeAccessChain& node) {
		const size_t idx = first_opt_index(node);
		const Token tok = node.opt_chaining_indexes[idx].has_value()
			? node.opt_chaining_indexes[idx].value() : node.tok;
		auto error = Diagnostic(ErrorType::SyntaxError, "", "", tok);
		error.message = "Optional chaining cannot be used where a value is required, because there is no value "
			"when the object is <nil>. It is currently supported for standalone method calls <obj?.method(x)> "
			"and assignment targets <obj?.member := value>. For value contexts, a fallback via nullish "
			"coalescing <obj?.member ?? fallback> is planned but not implemented yet.";
		return error;
	}
};
