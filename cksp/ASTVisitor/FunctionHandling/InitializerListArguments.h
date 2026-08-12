//
// Created by Mathias Vatter on 12.08.26.
//

#pragma once

#include "../ASTVisitor.h"

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
			if (param_index < 0 or param_index >= static_cast<int>(definition->header->get_num_params())) continue;
			hoist_argument(node, argument, *definition->header->get_param(param_index), statement);
		}
		return &node;
	}

	/// Declares the list as a local copy of the parameter in front of <statement> and hands the
	/// call a reference to it.
	void hoist_argument(NodeFunctionCall& call, const int argument_index, NodeDataStructure& parameter,
						NodeStatement* statement) {
		auto list = unique_ptr_cast<NodeInitializerList>(std::move(call.function->get_arg(argument_index)));
		const auto tok = list->tok;
		// A parameter that declares no size takes whatever the list holds - there is nothing to
		// pad the array to, and the callee reads no further either.
		const auto sizes = constant_sizes(parameter);
		int32_t num_elements = 0;
		if (sizes) {
			num_elements = 1;
			for (const auto size : *sizes) num_elements *= size;
		}
		const bool covers_every_element = !sizes or static_cast<int32_t>(list->size()) >= num_elements;

		auto array = sizes
			? clone_as<NodeDataStructure>(&parameter)
			: unique_ptr_cast<NodeDataStructure>(list->transform_to_array(""));
		array->name = m_def_provider->get_fresh_name("_arr");
		array->is_local = true;
		array->kind = NodeDataStructure::Kind::Throwaway;

		auto declaration = std::make_unique<NodeSingleDeclaration>(
			std::move(array),
			covers_every_element
				? unique_ptr_cast<NodeAST>(std::move(list))
				: std::make_unique<NodeInitializerList>(
					tok, TypeRegistry::get_neutral_element_from_type(parameter.ty->get_element_type())),
			tok);
		const auto declared = declaration->variable;

		auto& hoisted = m_hoisted_per_statement[statement];
		hoisted.push_back(std::move(declaration));
		if (!covers_every_element) {
			for (size_t element = 0; element < list->size(); ++element) {
				hoisted.push_back(std::make_unique<NodeSingleAssignment>(
					element_ref(*declared, *sizes, element, tok),
					std::move(list->elements[element]),
					tok));
			}
		}
		call.function->set_arg(argument_index, declared->to_reference());
	}

	/// Reference to the <element>th element, counted the way the list spells the array out: straight
	/// through the dimensions, so element 3 of a [2,3] array is <[1,0]>.
	static std::unique_ptr<NodeReference> element_ref(NodeDataStructure& array, const std::vector<int32_t>& sizes,
													  const size_t element, const Token& tok) {
		if (sizes.size() == 1) {
			auto reference = unique_ptr_cast<NodeArrayRef>(array.to_reference());
			reference->set_index(std::make_unique<NodeInt>(static_cast<int32_t>(element), tok));
			return reference;
		}
		auto indexes = std::make_unique<NodeParamList>(tok);
		auto remainder = static_cast<int32_t>(element);
		for (size_t dimension = 0; dimension < sizes.size(); ++dimension) {
			int32_t stride = 1;
			for (size_t rest = dimension + 1; rest < sizes.size(); ++rest) stride *= sizes[rest];
			indexes->add_param(std::make_unique<NodeInt>(remainder / stride, tok));
			remainder %= stride;
		}
		auto reference = unique_ptr_cast<NodeNDArrayRef>(array.to_reference());
		reference->set_indexes(std::move(indexes));
		return reference;
	}

	/// The declared sizes of a data structure, when every one of them is a literal.
	static std::optional<std::vector<int32_t>> constant_sizes(NodeDataStructure& data) {
		if (const auto array = data.cast<NodeArray>()) {
			const auto size = array->size ? array->size->cast<NodeInt>() : nullptr;
			if (!size) return std::nullopt;
			return std::vector{size->value};
		}
		if (const auto nd_array = data.cast<NodeNDArray>()) {
			if (!nd_array->sizes or nd_array->sizes->empty()) return std::nullopt;
			std::vector<int32_t> sizes;
			sizes.reserve(nd_array->sizes->size());
			for (const auto& size : nd_array->sizes->params) {
				const auto value = size->cast<NodeInt>();
				if (!value) return std::nullopt;
				sizes.push_back(value->value);
			}
			return sizes;
		}
		return std::nullopt;
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
