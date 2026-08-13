//
// Created by Mathias Vatter on 12.08.26.
//

#pragma once

#include "../ASTVisitor.h"
#include "../TypeInference.h"

/**
 * Moves an initializer list handed to a call into a local array declared in front of it.
 *
 * The array is a copy of the formal parameter, so it has the shape the callee copies from - a list
 * shorter than the parameter would otherwise leave that copy reading past its end. Only the
 * elements the list spells out are written: unlike a declaration, an argument list does not spread
 * a single value over everything. What it leaves out keeps the neutral value of its type, which is
 * why this runs before <ASTLowerTypes>: <nil> is still spellable here, and an array of objects
 * whose elements stay at 0 would read as object 0 everywhere.
 */
class InitializerListArguments final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	/// harvested per statement, declared in front of the statement they were taken from
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeAST>>> m_hoisted_per_statement;

public:
	explicit InitializerListArguments(NodeProgram* program) : m_def_provider(program->def_provider) {
		m_program = program;
	}

private:
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		node.global_declarations->accept(*this);
		if (node.init_callback) node.init_callback->accept(*this);
		for (const auto& callback : node.callbacks) {
			if (callback.get() != node.init_callback) callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		visit_all(node.statements, *this);
		insert_hoisted(node);
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		const auto definition = node.get_definition();
		if (!definition or node.is_builtin_kind()) return &node;
		if (!definition->visited) {
			definition->visited = true;
			definition->accept(*this);
		}

		const auto statement = node.get_parent_statement();
		if (!statement) return &node;
		const int param_offset = node.get_param_offset(definition.get());
		for (int argument = 0; argument < node.function->get_num_args(); argument++) {
			if (!node.function->get_arg(argument)->cast<NodeInitializerList>()) continue;
			const int param_index = argument + param_offset;
			if (param_index < 0 or param_index >= definition->header->get_num_params()) continue;
			hoist_initializer_list(node, argument, *definition->header->get_param(param_index), statement);
		}
		return &node;
	}

	/// Declares the list as a local copy of the parameter in front of <statement> and hands the
	/// call a reference to it.
	void hoist_initializer_list(NodeFunctionCall& call, const int argument_index, NodeDataStructure& parameter,
						NodeStatement* statement) {
		auto list = unique_ptr_cast<NodeInitializerList>(std::move(call.function->get_arg(argument_index)));
		const auto tok = list->tok;
		// The array takes the shape of the parameter, not of the list: the callee copies as many
		// elements as its parameter declares, and a shorter list would leave it reading past the
		// end. A parameter that declares no size (a plain array parameter) hands its lack of one
		// over as well, and the size is inferred from the list as before.
		auto array = clone_as<NodeDataStructure>(&parameter);
		array->name = m_def_provider->get_fresh_name("_arr");
		array->is_local = true;
		array->kind = NodeDataStructure::Kind::Throwaway;

		auto declaration = std::make_unique<NodeSingleDeclaration>(
			std::move(array),
			std::move(list),
			tok);
		TypeInference::initialize_pointer_declaration_with_nil(*declaration);
		const auto declared = declaration->variable;

		m_hoisted_per_statement[statement].push_back(std::move(declaration));
		call.function->set_arg(argument_index, declared->to_reference());
	}

	void insert_hoisted(NodeBlock& node) {
		if (m_hoisted_per_statement.empty()) return;
		std::vector<std::unique_ptr<NodeStatement>> statements;
		statements.reserve(node.statements.size());
		for (auto& statement : node.statements) {
			if (const auto hoisted = m_hoisted_per_statement.find(statement.get());
				hoisted != m_hoisted_per_statement.end()) {
				for (auto& declaration : hoisted->second) {
					statements.push_back(std::make_unique<NodeStatement>(std::move(declaration), statement->tok));
					statements.back()->parent = &node;
				}
				m_hoisted_per_statement.erase(hoisted);
			}
			statements.push_back(std::move(statement));
		}
		node.statements = std::move(statements);
	}
};
