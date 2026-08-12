//
// Created by Mathias Vatter on 12.08.26.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Moves an initializer list handed to a call into a local array declared in front of it.
 *
 * The array is sized after the formal parameter, not after the list: the callee copies as many
 * elements as its parameter declares, so a shorter list would leave it reading past the end.
 * Only the elements the list spells out are written - unlike a declaration, an argument list does
 * not spread a single value over everything. What it leaves out keeps the neutral value of its
 * type, which is why this runs before <ASTLowerTypes>: <nil> is still spellable here, and an
 * array of objects whose elements stay at 0 would read as object 0 everywhere.
 */
class InitializerListArguments final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	/// declarations to insert in front of the statement they were harvested from
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeAST>>> m_declarations_per_statement;

public:
	explicit InitializerListArguments(NodeProgram* program) : m_def_provider(program->def_provider) {
		m_program = program;
	}

private:
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		node.global_declarations->accept(*this);
		for (const auto& callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		const auto definition = node.get_definition();
		if (!definition or node.is_builtin_kind()) return &node;
		if (!definition->visited) {
			definition->visited = true;
			definition->accept(*this);
		}

		const auto statement = node.get_parent_statement();
		if (!statement) return &node;
		for (int i = 0; i < node.function->get_num_args(); i++) {
			auto& argument = node.function->get_arg(i);
			const auto init_list = argument->cast<NodeInitializerList>();
			if (!init_list) continue;
			const auto& parameter = definition->header->get_param(i + node.get_param_offset(definition.get()));
			hoist_argument(node, i, *init_list, *parameter, statement);
		}
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		visit_all(node.statements, *this);
		insert_declarations(node);
		return &node;
	}

	/// Replaces the list argument with a reference to a local array holding it.
	void hoist_argument(NodeFunctionCall& call, const int argument_index, NodeInitializerList& init_list,
						const NodeDataStructure& parameter, NodeStatement* statement) {
		auto array = init_list.transform_to_array(m_def_provider->get_fresh_name("_arr"));
		array->is_local = true;
		const auto num_elements = constant_num_elements(parameter);
		const bool covers_every_element = !num_elements or init_list.size() >= *num_elements;
		if (num_elements) resize(*array, *num_elements);

		auto& declarations = m_declarations_per_statement[statement];
		if (covers_every_element) {
			declarations.push_back(std::make_unique<NodeSingleDeclaration>(
				clone_as<NodeDataStructure>(array.get()),
				std::move(call.function->get_arg(argument_index)),
				init_list.tok));
		} else {
			// The elements the list leaves out keep the neutral value of their type. Integers and
			// strings already read that way, an array of objects has to be laid out with <nil>.
			auto neutral = TypeRegistry::get_neutral_element_from_type(array->ty->get_element_type());
			const bool needs_neutral_value = array->ty->get_element_type()->get_type_kind() == TypeKind::Object;
			declarations.push_back(std::make_unique<NodeSingleDeclaration>(
				clone_as<NodeDataStructure>(array.get()),
				needs_neutral_value
					? std::make_unique<NodeInitializerList>(init_list.tok, std::move(neutral))
					: nullptr,
				init_list.tok));
			for (size_t element = 0; element < init_list.size(); ++element) {
				auto target = unique_ptr_cast<NodeArrayRef>(array->to_reference());
				target->set_index(std::make_unique<NodeInt>(static_cast<int32_t>(element), init_list.tok));
				declarations.push_back(std::make_unique<NodeSingleAssignment>(
					std::move(target),
					std::move(init_list.elements[element]),
					init_list.tok));
			}
			call.function->set_arg(argument_index, array->to_reference());
		}
		if (covers_every_element) {
			call.function->set_arg(argument_index, array->to_reference());
		}
	}

	/// Number of elements a data structure holds, when every one of its sizes is a literal.
	static std::optional<int32_t> constant_num_elements(const NodeDataStructure& data) {
		if (const auto array = cast_node<NodeArray>(&data)) {
			const auto size = array->size ? array->size->cast<NodeInt>() : nullptr;
			return size ? std::optional(size->value) : std::nullopt;
		}
		if (const auto nd_array = cast_node<NodeNDArray>(&data)) {
			if (!nd_array->sizes or nd_array->sizes->empty()) return std::nullopt;
			int32_t num_elements = 1;
			for (const auto& size : nd_array->sizes->params) {
				const auto value = size->cast<NodeInt>();
				if (!value) return std::nullopt;
				num_elements *= value->value;
			}
			return num_elements;
		}
		return std::nullopt;
	}

	static void resize(NodeDataStructure& array, const int32_t num_elements) {
		if (const auto node_array = cast_node<NodeArray>(&array)) {
			node_array->set_size(std::make_unique<NodeInt>(num_elements, array.tok));
		}
	}

	void insert_declarations(NodeBlock& node) {
		if (m_declarations_per_statement.empty()) return;
		std::vector<std::unique_ptr<NodeStatement>> statements;
		statements.reserve(node.statements.size());
		for (auto& statement : node.statements) {
			const auto harvested = m_declarations_per_statement.find(statement.get());
			if (harvested != m_declarations_per_statement.end()) {
				for (auto& declaration : harvested->second) {
					statements.push_back(std::make_unique<NodeStatement>(std::move(declaration), statement->tok));
					statements.back()->parent = &node;
				}
				m_declarations_per_statement.erase(harvested);
			}
			statements.push_back(std::move(statement));
		}
		node.statements = std::move(statements);
	}
};
