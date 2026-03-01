//
// Created by Mathias Vatter on 31.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "ReturnFunctionRewriting/ReturnFunctionIsolation.h"
#include "ReturnFunctionRewriting/ReturnFunctionCallHoisting.h"
#include "ASTTemporaryPointerScope.h"


class ExpressionChainValidator final : public ASTVisitor {
	DefinitionProvider *m_def_provider = nullptr;
	bool m_is_expression_chain = true;
public:

	bool is_expression_chain(NodeAST& node) {
		m_is_expression_chain = true;
		node.accept(*this);
		return m_is_expression_chain;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		if (!m_is_expression_chain) return &node;

		node.function->accept(*this);
		auto definition = node.get_definition();
		if (!definition) {
			m_is_expression_chain = false;
			return &node;
		}

		if (definition->is_expression_function() || node.is_builtin_kind()) {
			m_is_expression_chain &= true;
		} else {
			m_is_expression_chain &= false;
		}
		return &node;
	}

};

class ASTReturnFunctionRewriting final : public ASTVisitor {
	DefinitionProvider *m_def_provider;
	NodeAST* m_just_hoisted = nullptr;
public:
	explicit ASTReturnFunctionRewriting(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	void do_rewriting(NodeProgram& node) {
		// do hoisting and return parameter promotion
		node.accept(*this);
		static ReturnFunctionCallHoisting hoisting;
		hoisting.visit(node);
		node.reset_function_visited_flag();
		node.debug_print();

		static ReturnFunctionIsolation isolation(m_program);
		isolation.do_return_function_isolation(node);
		node.debug_print();

		static ASTTemporaryPointerScope temp_scope(m_program);
		temp_scope.visit(node);

		node.update_function_lookup();
		node.reset_function_visited_flag();
	}

private:

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;

		m_program->global_declarations->accept(*this);
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		return &node;
	};

	NodeAST* visit(NodeFunctionDefinition& node) override {
		node.do_return_param_promotion(m_program);
		node.visited = true;
		node.header->accept(*this);
		if(node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if (!node.value) return &node;
		if (const auto func_call = node.value->cast<NodeFunctionCall>()) {
			func_call->bind_definition(m_program);
			const auto definition = func_call->get_definition();
			if (!definition) {
				node.value->accept(*this);
				return &node;
			}
			// Fast path: only declarations from return-value functions need special handling.
			if (definition->num_return_params <= 0) {
				node.value->accept(*this);
				return &node;
			}

			static ExpressionChainValidator expression_chain_validator;
			if (expression_chain_validator.is_expression_chain(*func_call)) {
				// do not hoist expression chains
				return node.value->accept(*this);
			}

			if (definition->num_return_params > 0) {
				auto node_block = std::make_unique<NodeBlock>(node.tok, false);
				auto node_assignment = std::make_unique<NodeSingleAssignment>(
					node.variable->to_reference(), std::move(node.value), node.tok);
				node_assignment->collect_references();
				auto node_decl = std::make_unique<NodeSingleDeclaration>(std::move(node.variable), nullptr, node.tok);
				node_block->add_as_stmt(std::move(node_decl));
				node_block->add_as_stmt(std::move(node_assignment));
				return node.replace_with(std::move(node_block))->accept(*this);
			}
		}
		node.value->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			const auto definition = node.get_definition();
			if(!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;


			// add throwaway variable ref to params
			// if (node.parent->cast<NodeStatement>() and &node != m_just_hoisted) {
			// 	if (node.is_builtin_kind()) return &node;
			// 	if (definition->num_return_params > 0) {
			// 		auto &throwaway_var = definition->header-
			// 		>get_param(0);
			// 		auto throwaway_ref = throwaway_var->to_reference();
			// 		throwaway_ref->name = m_def_provider->get_fresh_name("_");
			// 		throwaway_ref->kind = NodeReference::Kind::Throwaway;
			// 		// add declaration to global vars
			// 		auto throwaway_decl = std::make_unique<NodeSingleDeclaration>(
			// 			std::move(throwaway_var), nullptr, node.tok);
			// 		m_program->global_declarations->add_as_stmt(std::move(throwaway_decl));
			// 		node.function->prepend_arg(std::move(throwaway_ref));
			// 	}
			// }
		}
		return &node;
	}

	NodeAST * visit(NodeFunctionHeaderRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.get_declaration() and node.is_func_arg()) {
			if(const auto def = node.get_declaration()->parent->cast<NodeFunctionDefinition>()) {
				if(!def->visited) def->accept(*this);
				def->visited = true;
			}
		}
		if(node.args) node.args->accept(*this);
		return &node;
	}

	// checks if the function call is a chain of expression functions that do not get hoisted
	// static bool is_expression_chain(const NodeFunctionCall* node) {
	// 	bool is_chain = true;
	// 	auto definition = node->get_definition();
	// 	if (definition and definition->is_expression_function()) {
	// 		for (auto& arg : node->function->args->params) {
	// 			if (const auto func_call = arg->cast<NodeFunctionCall>()) {
	// 				auto def = func_call->get_definition();
	// 				if (def and def->is_expression_function()) {
	// 					is_chain &= is_expression_chain(func_call);
	// 				}
	// 			} else {
	// 				is_chain &= true;
	// 			}
	// 		}
	// 	}
	// 	return false;
	// }

};

