//
// Created by Mathias Vatter on 23.04.24.
//

#pragma once

#include "ASTLowering.h"
#include <limits>

class LoweringList final : public ASTLowering {
public:
	explicit LoweringList(NodeProgram* program) : ASTLowering(program) {}

	NodeAST * visit(NodeSingleDeclaration &node) override {
		if(auto list = node.variable->cast<NodeList>()) {

			std::string list_name = list->name;
			bool is_one_dim = !list->is_jagged;
			auto list_body = std::move(list->body);
			auto list_size = (int32_t)list_body.size();
			std::vector<int32_t> sizes(list_size);
			int64_t total_size = 0;
			for (int i = 0; i < list_size; ++i) {
				sizes[i] = static_cast<int32_t>(list_body[i]->size());
				if (auto source = array_row(*list_body[i])) {
					const auto declaration = source->get_declaration();
					const auto array = declaration ? declaration->cast<NodeArray>() : nullptr;
					if (!array || !array->size) {
						Diagnostic(ErrorType::TypeError, "List rows must be arrays with a known compile-time size.",
							"one-dimensional array", source->tok).exit();
					}
					array->size->do_constant_folding();
					const auto size = array->size->cast<NodeInt>();
					if (!size || size->value < 0) {
						Diagnostic(ErrorType::TypeError, "List row size must be a non-negative compile-time integer.",
							"constant array size", source->tok).exit();
					}
					sizes[i] = size->value;
				}
				total_size += sizes[i];
				if (total_size > std::numeric_limits<int32_t>::max()) {
					Diagnostic(ErrorType::TypeError, "List exceeds the supported array size.", "32-bit array size", list->tok).exit();
				}
			}
			list->size = static_cast<int32_t>(total_size);
			node.variable->accept(*this);

			auto node_body = std::make_unique<NodeBlock>(node.tok);

			// declare all_fx_settings[48]
			auto node_declare_main_array = std::make_unique<NodeSingleDeclaration>(
				node.variable,
				nullptr,
				node.tok
			);

			// declare const all_fx_settings.SIZE : int := 8
			auto node_declare_main_size = std::make_unique<NodeSingleDeclaration>(
				std::make_unique<NodeVariable>(
					std::nullopt,
					list_name+".SIZE",
					TypeRegistry::Integer,
					node.tok,
					DataType::Const
				),
				std::make_unique<NodeInt>(list_size, node.tok), node.tok);

			node_body->add_as_stmt(std::move(node_declare_main_size));

			// if list is one dimensional -> just declare main array
			if(is_one_dim) {
				// bring all one sized param lists into the first
				for(int i = 1; i<list_body.size(); i++) {
					list_body[0]->add_element(std::move(list_body[i]->elem(0)));
				}
				if (!list_body.empty()) node_declare_main_array->set_value(std::move(list_body[0]));
				node_body->add_as_stmt(std::move(node_declare_main_array));
				return node.replace_with(std::move(node_body));
			}

			node_body->add_as_stmt(std::move(node_declare_main_array));
			node.variable->name = "_"+node.variable->name;
			auto node_sizes_array = std::make_unique<NodeArray>(
				std::nullopt,
				list_name+".sizes",
				TypeRegistry::ArrayOfInt,
				std::make_unique<NodeInt>(list_size, node.tok), node.tok
			);
			auto node_positions_array = std::make_unique<NodeArray>(
				std::nullopt,
				list_name+".pos",
				TypeRegistry::ArrayOfInt,
				std::make_unique<NodeInt>(list_size, node.tok), node.tok
			);

			std::vector<int32_t> positions(list_body.size());
			auto node_sizes = std::make_unique<NodeInitializerList>(node.tok);
			auto node_positions = std::make_unique<NodeInitializerList>(node.tok);
			if (!positions.empty()) positions[0] = 0;
			for(int i = 0; i<list_body.size(); i++) {
				if(i>0) positions[i] = positions[i - 1] + sizes[i - 1];
				node_sizes->add_element(std::make_unique<NodeInt>(sizes[i], node.tok));
				node_positions->add_element(std::make_unique<NodeInt>(positions[i], node.tok));
			}
			auto node_sizes_declaration = std::make_unique<NodeSingleDeclaration>(
				std::move(node_sizes_array),
				list_size ? std::move(node_sizes) : nullptr, node.tok
			);
			auto node_positions_declaration = std::make_unique<NodeSingleDeclaration>(
				std::move(node_positions_array),
				list_size ? std::move(node_positions) : nullptr, node.tok
			);
			node_body->add_as_stmt(std::move(node_sizes_declaration));
			node_body->add_as_stmt(std::move(node_positions_declaration));

			const auto node_iterator_var = m_program->get_global_iterator()->to_reference();
			for(int i = 0; i<list_body.size(); i++) {
				auto source = array_row(*list_body[i]);
				std::string source_name;
				if (source) {
					source_name = source->name;
				} else {
					auto node_array = std::make_unique<NodeArray>(
						std::nullopt,
						m_def_provider->get_fresh_name(list_name + "_row"),
						TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1),
						std::make_unique<NodeInt>(sizes[i], node.tok), node.tok
					);
					source_name = node_array->name;
					auto node_array_declaration = std::make_unique<NodeSingleDeclaration>(
						clone_as<NodeDataStructure>(node_array.get()),
						std::move(list_body[i]),
						node.tok
					);
					node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_array_declaration), node.tok));
				}

				auto node_while_body = std::make_unique<NodeBlock>(node.tok);
				auto node_expression = std::make_unique<NodeBinaryExpr>(
					token::ADD,
					node_iterator_var->clone(),
					std::make_unique<NodeInt>(positions[i], node.tok),
					node.tok
				);
				node_expression->ty = TypeRegistry::Integer;

				auto node_array_ref = std::make_unique<NodeArrayRef>(
					source_name,
					node_iterator_var->clone(),  node.tok
				);
				auto node_main_array_ref = std::make_unique<NodeArrayRef>(
					"_"+list_name,
					std::move(node_expression), node.tok
				);
				auto node_assignment = std::make_unique<NodeSingleAssignment>(
					std::move(node_main_array_ref),
					std::move(node_array_ref), node.tok
				);
				node_while_body->add_as_stmt(std::move(node_assignment));
				auto node_while_loop = make_while_loop(node_iterator_var.get(), 0, sizes[i], std::move(node_while_body), node_body.get());
				node_body->add_as_stmt(std::move(node_while_loop));
			}
			node_body->accept(*this);
			return node.replace_with(std::move(node_body));
		}

		return &node;
	};

	NodeAST * visit(NodeList &node) override {
		auto node_main_array = std::make_unique<NodeArray>(
			std::nullopt,
			node.name,
			TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), 1),
			std::make_unique<NodeInt>(node.size,node.tok),
			node.tok
		);
		return node.replace_datastruct(std::move(node_main_array));
		// return node.replace_with(std::move(node_main_array));
	};

	NodeAST * visit(NodeListRef& node) override {
		if (!node.indexes) {
			auto name = node.name;
			if (const auto decl = node.get_declaration()) {
				if (const auto list = decl->cast<NodeList>(); list && list->is_jagged && !node.has_raw_spelling()) name = "_" + name;
				else if (decl->cast<NodeArray>()) name = decl->name;
			}
			return node.replace_reference(std::make_unique<NodeArrayRef>(name, nullptr, node.tok));
		}
		// list references can only have one or two (jagged lists) index
		if(node.indexes->size() != 2 && node.indexes->size() != 1) {
			auto error = Diagnostic(ErrorType::SyntaxError,"", "", node.tok);
			error.message = "Got wrong amount of index for <List>.";
			error.expected = "2";
			error.actual = std::to_string(node.indexes->params.size());
			error.exit();
		}

		auto lowered_list_ref = std::make_unique<NodeArrayRef>(
			node.name,
			nullptr, node.tok
		);
		// no heavy lowering needed if list is simple one dimensional array
		if(node.indexes->params.size() == 1) {
			lowered_list_ref->set_index(std::move(node.indexes));
			return node.replace_with(std::move(lowered_list_ref));
		}

		/**
		 * pre lowering:
		 * all_fx_texts[0,1]
		 *
		 * post lowering:
		 * _all_fx_texts[all_fx_texts.pos[0]+1]
		 */
		auto node_position_array = std::make_unique<NodeArrayRef>(
			node.name+".pos",
			std::move(node.indexes->param(0)), node.tok
		);

		auto node_expression = std::make_unique<NodeBinaryExpr>(
			token::ADD,
			std::move(node_position_array),
			std::move(node.indexes->param(1)), node.tok);
		node_expression->ty = TypeRegistry::Integer;

		lowered_list_ref->set_index(std::move(node_expression));
		lowered_list_ref->name = "_"+lowered_list_ref->name;
		return node.replace_reference(std::move(lowered_list_ref));
		// return node.replace_with(std::move(lowered_list_ref));
	}

private:
	static NodeArrayRef* array_row(const NodeInitializerList& row) {
		if (row.size() != 1) return nullptr;
		const auto array = row.elements[0]->cast<NodeArrayRef>();
		return array && !array->index ? array : nullptr;
	}
};
