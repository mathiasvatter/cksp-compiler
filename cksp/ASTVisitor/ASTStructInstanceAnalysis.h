//
// Created by Mathias Vatter on 28.06.26.
//

#pragma once

#include "ASTVisitor.h"

/**
 * Analyses where and how many constructors of each struct are called to better determine the
 * optimal heap size to allocate for each struct
 * Also marks temporary constructors that have to be deleted later on in ReturnFunctionRewriting
 * - determine max_individual_structs_var by looking at where the constructors are called:
 * - only determine if constructor is called in the on init callback or on persistence_changed
 * 		- constructor called in linear for loop -> max_individual_structs_var + for loop elements
 */
class ASTStructInstanceAnalysis: public ASTVisitor {
	std::vector<NodeLoop*> m_loop_stack;
	std::unordered_map<NodeStruct*, std::unique_ptr<NodeAST>> m_num_constructors;
	std::vector<NodeFunctionDefinition*> m_function_call_stack;
	std::unordered_set<NodeStruct*> m_unbounded_structs;

	const ConstantDatabase& database;

	bool is_static_callback() const {
		if (!m_program->current_callback) return true; // this is the case if we are in global_declarations
		if (m_program->current_callback == m_program->init_callback) return true;
		if (m_program->current_callback->begin_callback == "on persistence_changed") return true;
		return false;
	}

public:
	explicit ASTStructInstanceAnalysis(NodeProgram *main, const ConstantDatabase& database)
		: database(database) {
		m_program = main;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;

		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}

		for(auto & struct_def : m_num_constructors) {
			auto statement = std::make_unique<NodeStatement>(std::move(struct_def.second), Token());
			statement->do_constant_folding(&database);
			struct_def.first->max_individual_structs_count = std::move(statement->statement);
			struct_def.first->max_individual_structs_count->parent = struct_def.first;
		}

		return &node;
	}

	NodeAST* visit(NodeStruct& node)  override {
		node.members->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_program->current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFor& node) override {
		node.iterator->accept(*this);
		node.iterator_end->accept(*this);
		if(node.step) node.step->accept(*this);

		m_loop_stack.push_back(&node);
		node.body->accept(*this);
		m_loop_stack.pop_back();
		return &node;
	}

	NodeAST* visit(NodeForEach& node) override {
		if(node.key) node.key->accept(*this);
		if(node.value) node.value->accept(*this);
		node.range->accept(*this);

		m_loop_stack.push_back(&node);
		node.body->accept(*this);
		m_loop_stack.pop_back();
		return &node;
	}

	NodeAST* visit(NodeWhile& node) override {
		m_loop_stack.push_back(&node);
		node.condition->accept(*this);
		node.body->accept(*this);
		m_loop_stack.pop_back();
		return &node;
	}

	// if constructor, get struct and increase constructor count
	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		auto definition = node.get_definition();
		if(definition and node.kind != NodeFunctionCall::Kind::Builtin) {
			// Analyze every call site in its callback/loop context. The global visited flag
			// would otherwise discard calls reached through functions after their first use.
			// Only suppress a definition that is already active to avoid recursive traversal.
			if(std::ranges::find(m_function_call_stack, definition.get()) == m_function_call_stack.end()) {
				FunctionCallStackScope diagnostic_frame(*m_program, node);
				m_function_call_stack.push_back(definition.get());
				m_program->function_definition_stack.push(definition);
				definition->accept(*this);
				m_program->function_definition_stack.pop();
				m_function_call_stack.pop_back();
			}
		}

		if(node.kind == NodeFunctionCall::Kind::Constructor) {
			// temporary constructor only  when in access chain or func arg of func that is not constructor
			node.is_temporary_constructor = node.is_func_arg() || node.parent->cast<NodeAccessChain>();
			if(definition and definition->parent) {
				if(auto struct_def = definition->parent->cast<NodeStruct>()) {
					increase_num_constructors(struct_def);
				}
			}
		}

		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		return &node;
	}

private:
	static void add_expr_to_num_constructors(std::unordered_map<NodeStruct*, std::unique_ptr<NodeAST>> &map, NodeStruct* key, std::unique_ptr<NodeAST> expr) {
		if (expr == nullptr) return;
		map[key] = add(map[key], expr);
	}

	static std::unique_ptr<NodeAST> add(std::unique_ptr<NodeAST>& left, std::unique_ptr<NodeAST>& right) {
		if (!left and !right) return nullptr;
		if (!left) {
			return std::move(right);
		} else if (!right) {
			return std::move(left);
		}
		const Token tok = left->tok;
		return std::make_unique<NodeBinaryExpr>(token::ADD, std::move(left), std::move(right), tok);
	}

	[[nodiscard]] std::unique_ptr<NodeAST> resolve_fixed_value(const NodeAST& expr) const {
		auto statement = std::make_unique<NodeStatement>(expr.clone(), expr.tok);
		statement->do_constant_folding(&database);
		auto folded = std::move(statement->statement);
		folded->parent = nullptr;
		return folded->cast<NodeInt>() ? std::move(folded) : nullptr;
	}

	// multiply all loop iterations together if there are multiple loops
	[[nodiscard]] std::unique_ptr<NodeAST> determine_num_iterations() const {
		std::unique_ptr<NodeAST> all_num;
		for(auto loop: m_loop_stack) {
			auto num = loop->get_num_iterations();
			if (!num) return nullptr;
			num = resolve_fixed_value(*num);
			if (!num) return nullptr;
			const Token tok = num->tok;
			all_num = all_num
				? std::make_unique<NodeBinaryExpr>(token::MULT, std::move(all_num), std::move(num), tok)
				: std::move(num);
		}
		return all_num ? resolve_fixed_value(*all_num) : nullptr;
	}

	void mark_unbounded(NodeStruct* struct_def) {
		m_unbounded_structs.insert(struct_def);
		m_num_constructors.erase(struct_def);
	}

	void increase_num_constructors(NodeStruct* struct_def) {
		if (m_unbounded_structs.contains(struct_def)) return;

		// if we are in on init or persistence (and maybe in a function call stack)
		if (is_static_callback()) {
			std::unique_ptr<NodeAST> num_constructor_calls = std::make_unique<NodeInt>(1, Token());
			// if the constructor is called in a linear loop environment, add the number of iterations to the constructor count
			if (!m_loop_stack.empty()) {
				num_constructor_calls = determine_num_iterations();
				if (!num_constructor_calls) {
					mark_unbounded(struct_def);
					return;
				}
			}

			add_expr_to_num_constructors(m_num_constructors, struct_def, std::move(num_constructor_calls));
		} else {
			// A constructor reachable from a dynamic callback cannot be bounded by the
			// number of static call sites, so keep the regular fallback heap size.
			mark_unbounded(struct_def);
		}
	}
};
