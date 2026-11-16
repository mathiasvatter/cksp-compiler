#pragma once

#include "../ASTVisitor.h"

/**
 * Marks every function a callback or global declaration can call as <is_used>, without
 * touching anything else.
 *
 * The <PostLowering> variable check only enters functions marked this way. The compile path
 * gets the marks from MarkThreadSafe, which walks the same calls but also decides thread
 * safety and rejects asynchronous calls in <on init>. The analysis path stops before that
 * pass, and without the marks it never checked a function body at all: an undeclared name
 * inside a called function compiled to an error but showed nothing in the editor.
 */
class MarkReachableFunctions final : public ASTVisitor {
public:
	explicit MarkReachableFunctions(NodeProgram* main) {
		m_program = main;
	}

	/// The same roots MarkThreadSafe::mark_environments starts from, so both paths agree on
	/// which functions the variable check enters.
	void mark(NodeProgram& node) {
		m_program = &node;
		node.reset_function_visited_flag();
		node.reset_function_used_flag();
		node.global_declarations->accept(*this);
		node.init_callback->accept(*this);
		for (const auto& s : node.struct_definitions) {
			s->accept(*this);
		}
		for (const auto& callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
	}

private:
	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		if (node.is_builtin_kind()) return &node;
		if (const auto definition = node.get_definition(); definition && !definition->visited) {
			definition->visited = true;
			definition->is_used = true;
			definition->accept(*this);
		}
		return &node;
	}
};
