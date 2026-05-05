//
// Created by Mathias Vatter on 14.04.26.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"

/**
 * Reduces code size by replacing scalar variable declarations with a large array
 * every variable will be assigned a constant index in that array
 * pre pass:
 *	declare a : int := 4
 *	declare b : int := 5
 *	declare c: int
 *	declare d : int
 * post pass:
 *	declare __vars[4]: int[]
 *	__vars[0] := 4
 *	__vars[1] := 5
 * The following things have to hold for the AST:
 *  - all variable names must be unique
 * IMPORTANT: when watch_var was used it has to be transformed into watch_array_idx
 */
class ScalarVarToArray final : public ASTOptimizations {

	/// maps the variable names to their assigned index in the global int vars array
	std::unordered_map<std::string, int> m_variable_to_index;
	std::unique_ptr<NodeSingleDeclaration> m_int_var_array;
	std::atomic_int m_counter = 0;
	std::mutex m_mutex;
public:

	explicit ScalarVarToArray() {
		m_counter = 0;
	}

	NodeAST* do_parallel_traversal(NodeProgram& node) {
		m_counter = 0;
		m_int_var_array = nullptr;
		m_program = &node;

		m_int_var_array = std::make_unique<NodeSingleDeclaration>(
			std::make_shared<NodeArray>(
				std::nullopt,
				m_program->def_provider->get_fresh_name("__vars"),
				TypeRegistry::ArrayOfInt,
				nullptr,
				node.tok,
				DataType::Mutable
			),
			nullptr,
			node.tok
		);

		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		parallel_for_each(node.callbacks.begin(), node.callbacks.end(),
			[this](const auto & callback) {
				  if (callback.get() == m_program->init_callback) return;
				callback->accept(*this);
			}
		);
		parallel_for_each(node.function_definitions.begin(), node.function_definitions.end(),
			[this](const auto & func) {
				func->accept(*this);
			}
		);
		node.reset_function_visited_flag();

		auto size = std::make_unique<NodeInt>(m_counter, node.tok);
		m_int_var_array->variable->cast<NodeArray>()->set_size(std::move(size));
		node.init_callback->statements->prepend_as_stmt(std::move(m_int_var_array));
		return &node;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_counter = 0;
		m_int_var_array = nullptr;
		m_program = &node;

		m_int_var_array = std::make_unique<NodeSingleDeclaration>(
			std::make_shared<NodeArray>(
				std::nullopt,
				m_program->def_provider->get_fresh_name("__vars"),
				TypeRegistry::ArrayOfInt,
				nullptr,
				node.tok,
				DataType::Mutable
			),
			nullptr,
			node.tok
		);

		m_program->global_declarations->accept(*this);
		visit_all(node.callbacks, *this);
		visit_all(node.function_definitions, *this);
		node.reset_function_visited_flag();

		auto size = std::make_unique<NodeInt>(m_counter, node.tok);
		m_int_var_array->variable->cast<NodeArray>()->set_size(std::move(size));
		node.init_callback->statements->prepend_as_stmt(std::move(m_int_var_array));
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if (node.value) node.value->accept(*this);

		// transform into assignment -> not when ui control!
		if (node.variable->ty == TypeRegistry::Integer && !node.variable->cast<NodeUIControl>()) {
			// reconnect all its references to the int array
			const auto& refs = node.variable->references;
			// parallel_for_each(refs.begin(), refs.end(),
			//   [&](auto const& ref) {
			// 	ref->declaration = m_int_var_array->variable;
			// });
			for (auto &ref : refs) {
				ref->declaration = m_int_var_array->variable;
			}
			// make variable into array reference
			auto array_ref = m_int_var_array->variable->to_reference();
			array_ref->ty = node.variable->ty;

			// set variable name to index map
			m_variable_to_index[node.variable->name] = m_counter;
			auto const_index = std::make_unique<NodeInt>(m_counter, node.tok);
			array_ref->cast<NodeArrayRef>()->set_index(std::move(const_index));
			m_counter++;

			if (node.value) {
				auto assign_stmt = std::make_unique<NodeSingleAssignment>(
					std::move(array_ref),
					std::move(node.value),
					node.tok
				);
				return node.replace_with(std::move(assign_stmt));
			} else {
				return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
			}
		}

		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		auto decl = node.get_declaration();
		if (decl == m_int_var_array->variable) {
			const auto it = m_variable_to_index.find(node.name);
			if (it != m_variable_to_index.end()) {
				auto array_ref = m_int_var_array->variable->to_reference();
				array_ref->ty = node.ty;
				auto index = std::make_unique<NodeInt>(it->second, node.tok);
				array_ref->cast<NodeArrayRef>()->set_index(std::move(index));
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					return node.replace_reference(std::move(array_ref));
				}
			}
		}
		return &node;
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);

		// transform watch_var into watch_array_idx
		if (node.is_builtin_kind()) {
			if (node.function->get_num_args() == 1 and node.function->name == "watch_var") {
				auto &arg = node.function->get_arg(0);
				auto array_ref = arg->cast<NodeArrayRef>();
				if (!array_ref) {
					// can also be slider then it does not get transformed into an array reference
					// auto error = ASTVisitor::get_raw_compile_error(ErrorType::InternalError, node);
					// error.set_message("First argument of watch_var was not transformed into an array reference. This should have been done in the visit(NodeVariableRef) method of this optimization.");
					// error.exit();
					return &node;
				}
				auto index = std::move(array_ref->index);
				array_ref->remove_index();
				array_ref->ty = m_int_var_array->variable->ty;
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					array_ref->remove_references();
					auto watch_array_idx = DefinitionProvider::create_builtin_call("watch_array_idx", std::move(arg), std::move(index));
					watch_array_idx->collect_references();
					return node.replace_with(std::move(watch_array_idx));
				}
			}
			return &node;
		}
		// if (const auto& definition = node.get_definition()) {
		// 	if(!definition->visited) {
		// 		m_program->function_call_stack.emplace(definition);
		// 		definition->accept(*this);
		// 		m_program->function_call_stack.pop();
		// 	}
		// 	definition->visited = true;
		// }
		return &node;
	}


};