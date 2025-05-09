//
// Created by Mathias Vatter on 22.02.25.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * For all function calls -> promote the formal parameters to global variables (if they are not
 * marked as pass-by-reference)
 * Assign before each function call the actual parameters to the formal parameters
 *
 */
class ParameterAssignmentTransformation final : public ASTVisitor {

	DefinitionProvider* m_def_provider = nullptr;
public:
	explicit ParameterAssignmentTransformation(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	NodeAST* do_parameter_assignment(NodeProgram& node) {
		for (const auto & func_def : node.function_definitions) {
			func_def->accept(*this);
		}
		return &node;
	}

private:

	// for all function definitions, visit their calls and add an assignment (if not pass-by-reference)
	// from actual parameter to formal parameter
	// only keep the pass-by-reference parameters and arguments in calls -> modyfing number of parameters
	NodeAST* visit(NodeFunctionDefinition& node) override {

		// do not transform if the function is ExpressionFunc
		if (node.is_expression_function()) {
			return &node;
		}
		if (node.call_sites.empty()) {
			return &node;
		}

		std::vector<std::unique_ptr<NodeFunctionParam>> promoted_params;
		std::vector<int> promoted_param_indices;
		std::vector<std::unique_ptr<NodeFunctionParam>> reference_params;
		std::vector<int> reference_param_indices;
		for (int i = 0 ; i<node.header->params.size(); i++) {
			auto& param = node.header->params[i];
			if (param->is_pass_by_ref) {
				reference_params.push_back(std::move(param));
				reference_param_indices.push_back(i);
			} else {
				// if not pass-by-reference, promote to global var
				param->variable->is_local = false;
				auto decl = std::make_unique<NodeSingleDeclaration>(param->variable, std::move(param->value), param->tok);
				promoted_params.push_back(std::move(param));
				promoted_param_indices.push_back(i);
				m_program->global_declarations->add_as_stmt(std::move(decl));
			}
		}

		// if there are no parameters left in the header now and the strategy was ParameterStack -> Call
		// if promoted params empty, then no assignments need to be made, if there are reference params ->inlining
		node.header->params = std::move(reference_params);
		if (promoted_params.empty()) {
			return &node;
		}

		std::vector<NodeFunctionCall*> calls;
		calls.reserve(node.call_sites.size());
		for (auto call : node.call_sites) {
			calls.push_back(call);
		}

		// replace all call sites with the transformed function
		for (int i = 0 ; i<calls.size(); i++) {
			const auto& call = calls[i];

			if (call->function->get_num_args() != promoted_params.size()) {

			}
			auto block = std::make_unique<NodeBlock>(call->tok, true);
			// get assignments going
			for (int ii = 0; ii<promoted_params.size(); ii++) {
				const auto& formal_param = promoted_params[ii]->variable;
				auto actual_param = std::move(call->function->get_arg(promoted_param_indices[ii]));

				auto assignment = std::make_unique<NodeSingleAssignment>(
					formal_param->to_reference(),
                    std::move(actual_param),
                    call->tok
				);
				assignment->is_parameter_stack = true;
				assignment->kind = NodeInstruction::ParameterStack;
				block->add_as_stmt(std::move(assignment));
			}
			// now there might be nullptr in the call args. Collect the args by reference
			auto actual_params_by_ref = std::make_unique<NodeParamList>(call->tok);
			for (const int idx : reference_param_indices) {
				actual_params_by_ref->add_param(std::move(call->function->get_arg(idx)));
			}
			call->function->set_args(std::move(actual_params_by_ref));
			auto new_call = std::make_unique<NodeFunctionCall>(
				false,
				std::move(call->function),
				call->tok
			);
			new_call->definition = call->definition;
			new_call->kind = call->kind;
			new_call->strategy = call->strategy;
			if (new_call->strategy == NodeFunctionCall::ParameterStack and new_call->function->has_no_args()) {
				new_call->is_call = true;
				new_call->strategy = NodeFunctionCall::Call;
			}
			block->add_as_stmt(std::move(new_call));
			node.call_sites.erase(call);
			node.call_sites.insert(block->get_last_statement()->cast<NodeFunctionCall>());
            call->replace_with(std::move(block));
        }

		return &node;
	}

};
