//
// Created by Mathias Vatter on 17.08.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"

/*
 * Assumes that the AST has now global scope. Collects all references and variables and (in a final step)
 * relinks references to variables by name using the DefinitionProvider.
 * This is necessary to ensure that all references are correctly linked to their corresponding data structures.
 * Will also refill the lists of all data structures with their references.
 */
class ASTRelinkGlobalScope final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
public:
	explicit ASTRelinkGlobalScope(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();

		// erase all previously saved scopes
		m_def_provider->refresh_scopes();

		m_def_provider->refresh_data_vectors();

		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		for(const auto & s : node.struct_definitions) {
			s->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			if(callback.get() != m_program->init_callback) callback->accept(*this);
		}
		node.reset_function_visited_flag();

		m_def_provider->reserve_global_scope(
			m_def_provider->get_all_data_structures().size() + m_def_provider->external_variables.size()
		);

		relink_global_scope();
		resolve_case_collisions();

		return &node;
	}

	void relink_global_scope() const {
		for(auto & data_struct : m_def_provider->get_all_data_structures()) {
			if (auto data = data_struct.lock()) {
				// data->clear_references();
				m_def_provider->set_declaration(data, true);
			} else {
				auto error = CompileError(ErrorType::InternalError, "", "", Token());
				error.m_message = "Data structure has been deleted during relinking.";
				error.exit();
			}
		}
		for(auto & reference : m_def_provider->get_all_references()) {
			if (reference->get_declaration()) continue;
			auto new_declaration = m_def_provider->get_declaration(*reference);
			if(!new_declaration) {
				DefinitionProvider::throw_declaration_error(*reference).exit();
			}
			new_declaration->add_reference(reference);
			reference->match_data_structure(new_declaration);
		}
	}

	void resolve_case_collisions() const {
		const auto data_structure_count = m_def_provider->get_all_data_structures().size();
		std::unordered_set<std::string> lower_case_names;
		lower_case_names.reserve(data_structure_count);
		std::vector<NodeDataStructure*> data_structures_to_rename;
		data_structures_to_rename.reserve(data_structure_count);
		for (auto & data_struct : m_def_provider->get_all_data_structures()) {
			if (auto data = data_struct.lock()) {
				std::string lower_case_name = StringUtils::to_lower(data->name);
				if (lower_case_names.contains(lower_case_name)) {
					data_structures_to_rename.emplace_back(data.get());
				} else {
					lower_case_names.insert(lower_case_name);
				}
			} else {
				auto error = CompileError(ErrorType::InternalError, "", "", Token());
				error.m_message = "Data structure has been deleted during relinking.";
				error.exit();
			}
		}

		for (auto & data_struct : data_structures_to_rename) {
			std::string new_name = m_def_provider->get_fresh_name(data_struct->name);
			data_struct->name = new_name;
			for (auto & ref : data_struct->references) {
				ref->name = new_name;
			}
		}
	}

	NodeAST* visit(NodeVariable& node) override {
		m_def_provider->add_to_data_structures(node.get_shared());
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		m_def_provider->add_to_references(&node);
		return &node;
	}

	NodeAST* visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
		m_def_provider->add_to_data_structures(node.get_shared());
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		m_def_provider->add_to_references(&node);
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			if(node.is_builtin_kind()) return &node;
			const auto definition = node.get_definition();
			if (!definition -> visited) definition->body->accept(*this);
			node.get_definition()->visited = true;
		}
		return &node;
	}

};
