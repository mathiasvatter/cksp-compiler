//
// Created by Mathias Vatter on 20.05.25.
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
	std::vector<NodeFunctionCall*> m_function_call_stack;
	struct FunctionTransformData {
		std::vector<std::unique_ptr<NodeFunctionParam>> promoted_params;
		std::vector<int> promoted_param_indices;
		std::vector<int> reference_param_indices;
	};
	std::unordered_map<NodeFunctionDefinition*, FunctionTransformData> m_func_transform_data;
	std::unordered_map<NodeFunctionCall*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_global_declarations;

public:
	explicit ParameterAssignmentTransformation(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
	}

	NodeAST* do_parameter_assignment(NodeProgram& node) {
		node.current_callback = nullptr;
		node.reset_function_visited_flag();
		auto n = node.accept(*this);
		m_function_call_stack.clear();
		m_global_declarations.clear();

		return n;
	}

	/// takes a block and a function call and wraps the block around the function call, ready to replace
	/// the function call
	static void swap_call(NodeFunctionCall& old, const std::unique_ptr<NodeBlock>& new_block) {
		auto definition = old.get_definition();
		if (!definition) {
			auto error = CompileError(ErrorType::InternalError, "", "", old.tok);
			error.m_message = "FunctionAssignmentTransformation : Function node has to have a definition to be transformed.";
			error.exit();
		}
		// create new call for easy replacement
		auto new_call = std::make_unique<NodeFunctionCall>(
			false,
			std::move(old.function),
			old.tok
		);
		new_call->definition = old.definition;
		new_call->kind = old.kind;
		new_call->strategy = old.strategy;

		// if call was ParameterStack strategy and has no args anymore -> Call strategy
		if (new_call->strategy == NodeFunctionCall::ParameterStack and new_call->function->has_no_args()) {
			new_call->is_call = true;
			new_call->strategy = NodeFunctionCall::Call;
		}
		new_block->add_as_stmt(std::move(new_call));
		definition->call_sites.erase(&old);
		definition->call_sites.insert(new_block->get_last_statement()->cast<NodeFunctionCall>());
	}

private:

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(const auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_program->current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		auto definition = node.get_definition();
		if (!definition) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "FunctionAssignmentTransformation : Function node has to have a definition to be transformed.";
			error.exit();
			return &node;
		}
		if (node.is_builtin_kind()) return &node;


		if (!definition->visited) {
			definition->visited = true;
			m_function_call_stack.push_back(&node);

			definition->accept(*this);

			m_function_call_stack.pop_back();
		}


		auto block = std::make_unique<NodeBlock>(node.tok, true);

		// add global declaration if available
		auto it = m_global_declarations.find(&node);
		if (it != m_global_declarations.end()) {
			for (auto & decl : it->second) {
				block->add_as_stmt(std::move(decl));
			}
			block->scope = false;
			m_global_declarations.erase(it);
		}

		// check if there is transformation data available for this function
		auto it1 = m_func_transform_data.find(definition.get());
		if (it1 != m_func_transform_data.end()) {
			// transform call by creating a new block and add assign/declaration statements of actual param to formal param
			const auto& func_data = it1->second;
			// get assignments going
			for (int i = 0; i < func_data.promoted_params.size(); i++) {
				const auto &formal_param = func_data.promoted_params[i]->variable;
				auto actual_param = std::move(node.function->get_arg(func_data.promoted_param_indices[i]));

				// check if formal and actual param are the same
				if (formal_param->name != actual_param->get_string()) {
					auto assign = std::make_unique<NodeSingleAssignment>(
						formal_param->to_reference(),
						std::move(actual_param),
						node.tok
					);
					assign->kind = NodeInstruction::ParameterStack;
					block->add_as_stmt(std::move(assign));
				}

			}

			// now there might be nullptr in the call args. Collect the args by reference
			auto actual_params_by_ref = std::make_unique<NodeParamList>(node.tok);
			for (const int idx : func_data.reference_param_indices) {
				actual_params_by_ref->add_param(std::move(node.function->get_arg(idx)));
			}
			node.function->set_args(std::move(actual_params_by_ref));
		}

		if (!block->statements.empty()) {
			swap_call(node, block);
			return node.replace_with(std::move(block));
		}

		return &node;
	}

	// /// IMPORTANT: all parameters of type array are automatically passed by reference
	// NodeAST* visit(NodeFunctionParam& node) override {
	// 	if (node.variable->is_function_param()) {
	// 		if (node.variable->ty->cast<CompositeType>()) {
	// 			// if the param is an array, it is passed by reference
	// 			node.is_pass_by_ref = true;
	// 		}
	// 	}
	// 	return &node;
	// }

	NodeAST* visit(NodeFunctionDefinition& node) override {
		// if(node.visited) return &node;
		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);


		{
			if (node.is_expression_function()) return &node;
			if (node.has_no_params()) return &node;

			// sort out the pass-by-reference parameters and the params to promote
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
					// if not pass-by-reference, promote to global var directly above the function call
					param->variable->to_global();
					auto decl = std::make_unique<NodeSingleDeclaration>(param->variable, nullptr, param->tok);
					decl->kind = NodeInstruction::ParameterStack;
					promoted_params.push_back(std::move(param));
					promoted_param_indices.push_back(i);
					// if the call is currently in the on init callback, do not move to global_declarations but directly
					if (m_program->current_callback == m_program->init_callback) {
						m_global_declarations[m_function_call_stack[0]].push_back(std::move(decl));
					} else {
						m_program->global_declarations->add_as_stmt(std::move(decl));
					}

				}
			}

			node.header->params = std::move(reference_params);
			// if there are no promoted params, return -> all params are pass-by-reference
			if (promoted_params.empty()) {
				return &node;
			}

			m_func_transform_data[&node] = {
				std::move(promoted_params),
				std::move(promoted_param_indices),
				std::move(reference_param_indices)
			};
		}




		return &node;
	}

};
