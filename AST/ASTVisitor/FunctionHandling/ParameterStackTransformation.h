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
class ParameterStackTransformation final : public ASTVisitor {

	DefinitionProvider* m_def_provider = nullptr;
public:
	explicit ParameterStackTransformation(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	NodeAST* do_function_stack_transformation(NodeProgram& node) {
		for (int i = 0; i<node.function_definitions.size(); i++) {
			const auto &func_def = node.function_definitions[i];
			// if (func_def->header->name == "floatmask.get_range") {
			//
			// }
			func_def->accept(*this);
		}
		node.merge_function_definitions();
		return &node;
	}

private:

	NodeAST* visit(NodeFunctionDefinition& node) override {
		// check if any call site has paramter stack strategy
		std::vector<NodeFunctionCall*> calls;
		for (const auto &call : node.call_sites) {
			if (call->strategy == NodeFunctionCall::Strategy::ParameterStack) {
				calls.push_back(call);
			}
		}
		if (calls.empty()) return &node;
		std::cout<< "Transforming function "<<node.header->name<<" to parameter stack strategy"<<std::endl;
		const std::string new_name = m_def_provider->get_fresh_name("called_"+node.header->name);
		auto new_def = clone_shared<NodeFunctionDefinition>(node.get_shared());
		new_def->update_parents(new_def.get());
		// auto new_def = to_unique_ptr(clone);

		new_def->call_sites.clear();
		new_def->is_used = true;
		new_def->visited = false;
		new_def->header->name = new_name;
		new_def->collect_declarations(m_program);
		new_def->remove_references();
		new_def->collect_references();

		std::vector<std::shared_ptr<NodeDataStructure>> promoted_params;
		for (const auto &param : new_def->header->params) {
			param->variable->name = m_def_provider->get_fresh_name(param->variable->name);
			for (auto &ref : param->variable->references) {
                ref->name = param->variable->name;
            }
			param->variable->is_local = false;
			auto decl = std::make_unique<NodeSingleDeclaration>(param->variable, std::move(param->value), param->tok);
			promoted_params.push_back(param->variable);
            m_program->global_declarations->add_as_stmt(std::move(decl));
		}
		new_def->header->params.clear();
		m_program->add_function_definition(std::move(new_def));

		// replace all call sites with the transformed function
		for (int i = 0; i<calls.size(); i++) {
			const auto &call = calls[i];
			if (call->strategy != NodeFunctionCall::Strategy::ParameterStack) continue;

			auto block = std::make_unique<NodeBlock>(call->tok, true);
			/// block after the call to get the formal params back to the actual params in case of variables (side effects)
			auto block_after = std::make_unique<NodeBlock>(call->tok, false);
			// get assignments going
			for (int ii = 0; ii<promoted_params.size(); ii++) {
				const auto& formal_param = promoted_params[ii];
				auto actual_param = std::move(call->function->get_arg(ii));
				if (const auto ref = cast_node<NodeReference>(actual_param.get())) {
					auto assignment_back = std::make_unique<NodeSingleAssignment>(
							clone_as<NodeReference>(ref),
							formal_param->to_reference(),
							call->tok
					);
					assignment_back->is_parameter_stack = true;
					block_after->add_as_stmt(std::move(assignment_back));
				}
				auto assignment = std::make_unique<NodeSingleAssignment>(
					formal_param->to_reference(),
                    std::move(actual_param),
                    call->tok
				);
				assignment->is_parameter_stack = true;
				block->add_as_stmt(std::move(assignment));
			}
			call->function->args->params.clear();
			call->function->name = new_name;
			call->definition.reset();
			call->bind_definition(m_program, true);
			call->is_call = true;
			call->strategy = NodeFunctionCall::Strategy::Call;
			block->add_as_stmt(call->clone());
			if (!block_after->statements.empty())
				block->append_body(std::move(block_after));
            call->replace_with(std::move(block));
        }



		return &node;
	}

};
