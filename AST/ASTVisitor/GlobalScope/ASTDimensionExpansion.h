//
// Created by Mathias Vatter on 27.11.24.
//

#pragma once

#include "MarkThreadSafe.h"
#include "../ASTVisitor.h"

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
 * Max length of event queue in Kontakt is 8192 (polyphonic var size)
 */
class ASTDimensionExpansion final : public ASTVisitor {

	int m_cb_stack_size = 100;
	DefinitionProvider* m_def_provider;


	[[nodiscard]] bool is_thread_safe_env() const {
		return (m_program->current_callback and m_program->current_callback->is_thread_safe) or
			(m_program->get_curr_function() and m_program->get_curr_function()->is_thread_safe);
	}

	NodeAST* mark_thread_safe(NodeProgram& node) const {
		// first mark threadsafe environments
		static MarkThreadSafe marker(m_program);
		for(const auto & callback : node.callbacks) {
			callback->accept(marker);
		}
		node.reset_function_visited_flag();
		return &node;
	}
public:
	explicit ASTDimensionExpansion(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
		m_program->current_callback = nullptr;

		m_program->max_cb_stack = std::make_shared<NodeVariable>(
			std::nullopt,
			"MAX::CB::STACK",
			TypeRegistry::Integer,
			DataType::Const,
			Token()
		);
		m_program->max_cb_stack->is_global = true;

		m_program->cb_idx = std::make_unique<NodeBinaryExpr>(
			token::MODULO,
			m_def_provider->get_builtin_variable("NI_CALLBACK_ID")->to_reference(),
			m_program->max_cb_stack->to_reference(),
			Token()
		);
		m_program->cb_idx->ty = TypeRegistry::Integer;
	}

	NodeAST* visit(NodeProgram &node) override {
		node.reset_function_visited_flag();
		m_program = &node;

		// add MAX::CB::STACK := 1000 to init callback
		auto decl = std::make_unique<NodeSingleDeclaration>(
			m_program->max_cb_stack,
			std::make_unique<NodeInt>(m_cb_stack_size, Token()),
			Token()
		);
		node.global_declarations->prepend_as_stmt(std::move(decl));

		mark_thread_safe(node);

		m_program->global_declarations->accept(*this);
		for(const auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;
		if(!is_thread_safe_env()) {
			if(node.callback_id) node.callback_id->accept(*this);
			node.statements->accept(*this);
		}
		m_program->current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			const auto definition = node.get_definition();
			if(!definition->visited and !definition->is_thread_safe) {
				m_program->function_call_stack.push(definition);
				definition->accept(*this);
				m_program->function_call_stack.pop();
			}
			definition->visited = true;
		}

		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) node.value->accept(*this);

		const bool is_thread_safe = is_thread_safe_env();
		node.variable->is_thread_safe = is_thread_safe;
		if(!is_thread_safe and node.variable->is_local) {
			// if value -> change to assignment
			std::unique_ptr<NodeBlock> block = nullptr;
			std::unique_ptr<NodeSingleAssignment> assignment = nullptr;
			if(node.value) {
				block = std::make_unique<NodeBlock>(node.tok);
				assignment = node.to_assign_stmt();
				// important for replace_datastruct afterwards to set new declaration pointer in reference
				node.variable->references.emplace(assignment->l_value.get());
			}

			auto inflated = node.variable->inflate_dimension(m_program->max_cb_stack->to_reference());
			// inflated->is_thread_safe = false;
			node.variable->replace_datastruct(std::move(inflated));

			if(node.value) {
				assignment->accept(*this);

				block->add_as_stmt(std::make_unique<NodeSingleDeclaration>(node.variable, nullptr, node.tok));
				block->add_as_stmt(std::move(assignment));
				block->collect_references();
				return node.replace_with(std::move(block));
			}

		}

		return &node;
	}

	bool determine_expansion_need(const NodeReference& ref) const {
		if(is_thread_safe_env()) return false;
		if(ref.kind != NodeReference::User) return false;
		const auto declaration = ref.get_declaration();
		if(!declaration) {
			auto error = CompileError(ErrorType::InternalError, "DimensionExpansion : Reference has no declaration", "", ref.tok);
//			error.exit();
			return false;
		}
		if(declaration->is_local and !declaration->is_function_param()) {
			return true;
		}
		return false;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if(!determine_expansion_need(node)) return &node;
		auto expanded = node.expand_dimension(m_program->cb_idx->clone());
		return node.replace_reference(std::move(expanded));
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);

		if(!determine_expansion_need(node)) return &node;

		auto expanded = node.expand_dimension(m_program->cb_idx->clone());
		return node.replace_reference(std::move(expanded));
	}

	NodeAST* visit(NodeNDArrayRef& node) override {
		if(node.indexes) node.indexes->accept(*this);
		if(node.sizes) node.sizes->accept(*this);

		if(!determine_expansion_need(node)) return &node;

		auto expanded = node.expand_dimension(m_program->cb_idx->clone());
		return node.replace_reference(std::move(expanded));
	}

	NodeAST* visit(NodeBlock &node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	}


};