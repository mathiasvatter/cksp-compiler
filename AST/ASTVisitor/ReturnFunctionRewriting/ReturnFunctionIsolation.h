//
// Created by Mathias Vatter on 22.11.24.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Isolates return functions in assignments and declarations into their own statements
 * Pre Lowering Assignment:
 * 	var := func_call()
 * Post Lowering Assignment:
 * 	func_call(var)
 * Pre Lowering Declaration:
 * 	declare var := func_call()
 * Post Lowering Declaration:
 * 	declare var;
 * 	func_call(var);
 * Pre Lowering Function Call with return value and no assign/declare statement:
 *  func_call()
 * Post Lowering Function Call with return value and no assign/declare statement:
 *  func_call(_);
 */
class ReturnFunctionIsolation final : public ASTVisitor {
	DefinitionProvider *m_def_provider;
public:
	explicit ReturnFunctionIsolation(NodeProgram *program) : m_def_provider(program->def_provider) {
		m_program = program;
	};

	void do_return_function_isolation(NodeProgram &node) {
		m_program = &node;
		node.reset_function_visited_flag();
		node.accept(*this);
		node.reset_function_visited_flag();
		node.update_function_lookup();
	}

private:
	NodeAST *visit(NodeProgram &node) override {
		m_program->global_declarations->accept(*this);
		for (auto &callback : node.callbacks) {
			callback->accept(*this);
		}
		return &node;
	};

	/// adds throwaway var as argument to call if no assign or declare statement
	NodeAST *visit(NodeFunctionCall &node) override {
		node.function->accept(*this);
		if (node.bind_definition(m_program)) {
			const auto definition = node.get_definition();
			if (!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;

			if (definition->is_expression_function()) return &node;

			// // add throwaway variable ref to params
			// if (node.parent->cast<NodeStatement>()) {
			// 	if (node.is_builtin_kind()) return &node;
			// 	if (definition->num_return_params > 0) {
			// 		auto &throwaway_var = definition->header->get_param(0);
			// 		auto throwaway_ref = throwaway_var->to_reference();
			// 		throwaway_ref->name = m_def_provider->get_fresh_name("_");
			// 		throwaway_ref->kind = NodeReference::Kind::Throwaway;
			// 		node.function->prepend_arg(std::move(throwaway_ref));
			// 	}
			// }
		}

		return &node;
	}

	NodeAST *visit(NodeFunctionHeaderRef &node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if (node.get_declaration() and node.is_func_arg()) {
			if (auto def = node.get_declaration()->parent->cast<NodeFunctionDefinition>()) {
				if (!def->visited) def->accept(*this);
				def->visited = true;
			}
		}
		if (node.args) node.args->accept(*this);
		return &node;
	}

	// if assigned functioncall has return parameter -> make it argument
	// replace single assignment with function call
	// Pre Lowering:
	// var := func_call()
	// Post Lowering:
	// func_call(var)
	NodeAST *visit(NodeSingleAssignment &node) override {
		node.r_value->accept(*this);
		node.l_value->accept(*this);
		if (const auto func_call = node.r_value->cast<NodeFunctionCall>()) {
			func_call->bind_definition(m_program);
			const auto definition = func_call->get_definition();
			if (!definition) return &node;
			if (definition->is_expression_function()) return &node;
			if (func_call->is_builtin_kind()) return &node;
			if (definition->num_return_params > 0) {
				node.remove_references();
				func_call->function->prepend_arg(std::move(node.l_value));
				return node.replace_with(std::move(node.r_value));
			}
		}
		return &node;
	}

	// Pre Lowering:
	// declare var := func_call()
	// Post Lowering:
	// declare var;
	// func_call(var);
	NodeAST *visit(NodeSingleDeclaration &node) override {
		if (!node.value) return &node;
		node.value->accept(*this);
		if (const auto func_call = node.value->cast<NodeFunctionCall>()) {
			func_call->bind_definition(m_program);
			const auto definition = func_call->get_definition();
			if (!definition) return &node;
			if (definition->is_expression_function()) return &node;
			if (func_call->is_builtin_kind()) return &node;
			if (definition->num_return_params > 0) {
				func_call->function->prepend_arg(node.variable->to_reference());
				node.remove_references();
				auto node_block = std::make_unique<NodeBlock>(node.tok, false);
				auto node_decl = std::make_unique<NodeSingleDeclaration>(std::move(node.variable), nullptr, node.tok);
				node_decl->kind = NodeSingleDeclaration::Kind::ReturnVar;
				node_block->add_as_stmt(std::move(node_decl));
				node_block->add_as_stmt(std::move(node.value));
				return node.replace_with(std::move(node_block));
			}
		}
		return &node;
	}

};
