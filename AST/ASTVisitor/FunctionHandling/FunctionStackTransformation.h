//
// Created by Mathias Vatter on 22.02.25.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Takes an isolated function call with parameters (no local variables or return statements) and turns
 * it into a KSP native function by promoting its formal parameters to global variables.
 *
 */
class FunctionStackTransformation final : public ASTVisitor {

	DefinitionProvider* m_def_provider = nullptr;
public:
	explicit FunctionStackTransformation(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	NodeAST* do_function_stack_transformation(NodeProgram& node) {
		for (int i = 0; i<node.function_definitions.size(); i++) {
			const auto &func_def = node.function_definitions[i];
			func_def->accept(*this);
		}
		return &node;
	}

private:

	NodeAST* visit(NodeFunctionDefinition& node) override {

		auto new_def = clone_as<NodeFunctionDefinition>(&node);
		new_def->header->name = m_def_provider->get_fresh_name("called_"+node.header->name);
		new_def->do_variable_checking(m_program, false);
		new_def->remove_references();
		new_def->collect_references();

		std::vector<NodeDataStructure*> promoted_params;
		for (const auto &param : new_def->header->params) {
			param->variable->name = m_def_provider->get_fresh_name(param->variable->name);
			for (auto &ref : param->variable->references) {
                ref->name = param->variable->name;
            }
			auto decl = std::make_unique<NodeSingleDeclaration>(std::move(param->variable), std::move(param->value), param->tok);
			promoted_params.push_back(decl->variable.get());
            m_program->global_declarations->add_as_stmt(std::move(decl));
		}

		m_program->add_function_definition(std::move(new_def));

		// replace all call sites with the transformed function
		for (auto &call : node.call_sites) {
			auto block = std::make_unique<NodeBlock>(call->tok, true);
			// get assignments going
			for (int i = 0; i<promoted_params.size(); i++) {
				const auto formal_param = promoted_params[i];
				auto actual_param = std::move(call->function->get_arg(i));
				auto assignment = std::make_unique<NodeSingleAssignment>(
					formal_param->to_reference(),
                    actual_param,
                    call->tok
				);
				block->add_as_stmt(std::move(assignment));
			}
			call->function->args.reset();
			call->bind_definition(m_program, true);

			block->add_as_stmt(call->clone());
            call->replace_with(std::move(block));
        }



		return &node;
	}

};
