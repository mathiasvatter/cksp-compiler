//
// Created by Mathias Vatter on 27.11.24.
//

#pragma once

#include "ASTVisitor.h"

/**
 * declare const MAX_CB_STACK := 1000
 * define CB_IDX := NI_CALLBACK_ID mod MAX_CB_STACK
 * on ui_controls()
 *     declare i[MAX_CB_STACK]: int
 *     message(i[CB_IDX])
 *     if(i[CB_IDX] = 1)
 *         declare arr[MAX_CB_STACK, 5] := (1,2,3,4)
 *         declare j[MAX_CB_STACK]: int
 *         declare k[MAX_CB_STACK]: int
 *         message(j[CB_IDX] & k[CB_IDX] & arr[CB_IDX, 3])
 *     end if
 *     if(i[CB_IDX] = 2)
 *         declare h[MAX_CB_STACK]: int
 *         declare g[MAX_CB_STACK]: int
 *         message(h[CB_IDX] & g[CB_IDX])
 *     end if
 * end on
 */
class ASTDimensionInflation : public ASTVisitor {
private:
	int m_cb_stack_size = 100;
	DefinitionProvider* m_def_provider;
	std::shared_ptr<NodeVariable> m_max_cb_stack;
	std::unique_ptr<NodeBinaryExpr> m_cb_idx;

	bool inline is_thread_safe_env() {
		return (m_program->current_callback and m_program->current_callback->is_thread_safe) or
			(m_program->get_curr_function() and m_program->get_curr_function()->is_thread_safe);
	};
public:
	inline explicit ASTDimensionInflation(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
		m_program->current_callback = nullptr;

		m_max_cb_stack = std::make_shared<NodeVariable>(
			std::nullopt,
			"MAX::CB::STACK",
			TypeRegistry::Integer,
			DataType::Const,
			Token()
		);
		m_max_cb_stack->is_global = true;

		m_cb_idx = std::make_unique<NodeBinaryExpr>(
			token::MODULO,
			m_def_provider->get_builtin_variable("NI_CALLBACK_ID")->to_reference(),
			m_max_cb_stack->to_reference(),
			Token()
		);
		m_cb_idx->ty = TypeRegistry::Integer;
	}

	inline NodeAST* visit(NodeProgram &node) override {
		node.reset_function_visited_flag();
		m_program = &node;

		// add MAX::CB::STACK := 1000 to init callback
		auto decl = std::make_unique<NodeSingleDeclaration>(
			m_max_cb_stack,
			std::make_unique<NodeInt>(m_cb_stack_size, Token()),
			Token()
		);
		node.init_callback->statements->prepend_as_stmt(std::move(decl));

		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	inline NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;
		if(!is_thread_safe_env()) {
			if(node.callback_id) node.callback_id->accept(*this);
			node.statements->accept(*this);
		}
		m_program->current_callback = nullptr;
		return &node;
	}

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			auto definition = node.get_definition();
			if(!definition->visited and !definition->is_thread_safe) {
				m_program->function_call_stack.push(definition);
				definition->accept(*this);
				m_program->function_call_stack.pop();
			}
			definition->visited = true;
		}

		return &node;
	}

	inline NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) node.value->accept(*this);

		if(!is_thread_safe_env() and node.variable->is_local) {
			// if value -> change to assignment
			std::unique_ptr<NodeBlock> block = nullptr;
			std::unique_ptr<NodeSingleAssignment> assignment = nullptr;
			if(node.value) {
				block = std::make_unique<NodeBlock>(node.tok);
				assignment = node.to_assign_stmt();
				// accept first to transform and inflate dimensions
				assignment->accept(*this);
			}

			auto inflated = node.variable->inflate_dimension(m_max_cb_stack->to_reference());
			node.variable->replace_datastruct(std::move(inflated));

			if(node.value) {
				block->add_as_stmt(std::make_unique<NodeSingleDeclaration>(node.variable, nullptr, node.tok));
				block->add_as_stmt(std::move(assignment));
				return node.replace_with(std::move(block));
			}

		}

		return &node;
	}

	inline bool determine_inflation_need(const NodeReference& ref) {
		if(is_thread_safe_env()) return false;
		if(ref.kind != NodeReference::User) return false;
		if(!ref.get_declaration()) {
			auto error = CompileError(ErrorType::InternalError, "Reference has no declaration", "", ref.tok);
//			error.exit();
			return false;
		}
		if(ref.get_declaration()->is_local) return true;
		return false;
	}

	inline NodeAST* visit(NodeVariableRef& node) override {
		if(!determine_inflation_need(node)) return &node;

		auto inflated = node.inflate_dimension(m_cb_idx->clone());
		return node.replace_reference(std::move(inflated));
	}

	inline NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);

		if(!determine_inflation_need(node)) return &node;

		auto inflated = node.inflate_dimension(m_cb_idx->clone());
		return node.replace_reference(std::move(inflated));
	}

	inline NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);

		if(!determine_inflation_need(node)) return &node;

		auto inflated = node.inflate_dimension(m_cb_idx->clone());
		return node.replace_reference(std::move(inflated));
	}

	inline NodeAST* visit(NodeBlock &node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	}


};