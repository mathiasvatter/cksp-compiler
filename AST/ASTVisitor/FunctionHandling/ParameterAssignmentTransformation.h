//
// Created by Mathias Vatter on 20.05.25.
//

#pragma once

#include "ParameterReuse.h"
#include "../ASTVisitor.h"

/**
 * For all function calls -> promote the formal parameters to global variables (if they are not
 * marked as pass-by-reference)
 * Assign before each function call the actual parameters to the formal parameters
 *
 */
class ParameterAssignmentTransformation final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	std::string throwaway_array_name{};
	std::vector<NodeFunctionCall*> m_function_call_stack{};
	struct FunctionTransformData {
		std::vector<std::unique_ptr<NodeFunctionParam>> promoted_params;
		std::vector<int> promoted_param_indices;
		std::vector<int> reference_param_indices;
	};
	std::unordered_map<NodeFunctionDefinition*, FunctionTransformData> m_func_transform_data;
	std::unordered_map<NodeFunctionCall*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_global_declarations;

	// those are all functions that get called within one function definition -> they depend on each other
	std::unordered_map<NodeFunctionDefinition*, std::unordered_set<NodeFunctionDefinition*>> m_subcalls_per_function;
	std::unordered_map<NodeFunctionDefinition*, std::vector<NodeDataStructure*>> m_param_decl_per_function;

public:
	explicit ParameterAssignmentTransformation(NodeProgram* main) {
		m_program = main;
		m_def_provider = main->def_provider;
		throwaway_array_name = m_def_provider->get_fresh_name("_arr");
	}

	NodeAST* do_parameter_assignment(NodeProgram& node) {
		node.current_callback = nullptr;
		node.reset_function_visited_flag();
		auto n = node.accept(*this);
		m_function_call_stack.clear();
		m_global_declarations.clear();
		node.debug_print();

		// node.reset_function_visited_flag();
		// node.remove_references();
		// node.collect_references();
		node.debug_print();
		// for (auto& fun : node.function_definitions) {
		// 	if (fun->header->name == "cc_on_controller") {
		// 		auto& set = m_subcalls_per_function[fun.get()];
		// 		for (auto & subcall : set) {
		// 			std::cout << subcall->header->name << std::endl;
		// 		}
		// 	}
		// }

		static ParameterReuse reuse(&node);
		reuse.do_parameter_reuse(
			m_subcalls_per_function,
			m_param_decl_per_function
		);
		node.debug_print();

		node.reset_function_visited_flag();
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
		if (!m_function_call_stack.empty()) {
			// // add all subcalls per function to this definition (since it will not be visited)
			// auto current_func =  m_function_call_stack.back()->get_definition().get();
			// auto it = m_subcalls_per_function.find(definition.get());
			// if (it != m_subcalls_per_function.end()) {
			// 	m_subcalls_per_function[current_func].insert(it->second.begin(), it->second.end());
			// }

			// insert all function definitions currently on the call stack to subcalls per function
			// add all definitions on the callstack to all other functions as subcalls
			for (const auto& call : m_function_call_stack) {
				auto existing_def = call->get_definition().get();
				auto current_def = definition.get();
				if (existing_def) {
					m_subcalls_per_function[current_def].insert(existing_def);
					m_subcalls_per_function[existing_def].insert(current_def);
				}
			}
		}


		auto block = std::make_unique<NodeBlock>(node.tok, true);
		auto return_block = std::make_unique<NodeBlock>(node.tok, false);

		// add global declaration if available
		auto it = m_global_declarations.find(&node);
		if (it != m_global_declarations.end()) {
			for (auto & decl : it->second) {
				block->add_as_stmt(std::move(decl));
			}
			block->scope = false;
			m_global_declarations.erase(it);
		}

		// if func args are init list -> replace with local array variable
		if (ASTFunctionStrategy::is_initializer_function(node)) {
			for (int i = 0; i < node.function->get_num_args(); i++) {
				auto& arg = node.function->get_arg(i);
				if (auto init_list = arg->cast<NodeInitializerList>()) {
					auto decl = std::make_unique<NodeSingleDeclaration>(
						init_list->transform_to_array(m_def_provider->get_fresh_name("_arr")),
						std::move(arg),
						arg->tok
					);
					decl->variable->is_local = true;
					auto ref = decl->variable->to_reference();
					node.function->set_arg(i, std::move(ref));
					block->add_as_stmt(std::move(decl));
				}
			}
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

				// return parameters have to passed back and do not need to be assigned
				if (formal_param->data_type == DataType::Return) {
					if (const auto ref = cast_node<NodeReference>(actual_param.get())) {
						auto return_assign = std::make_unique<NodeSingleAssignment>(
							unique_ptr_cast<NodeReference>(std::move(actual_param)),
							formal_param->to_reference(),
							node.tok
						);
						return_assign->kind = NodeInstruction::ReturnVar;
						return_assign->collect_references();
						return_block->add_as_stmt(std::move(return_assign));
						continue;
					}
				}

				// check if formal and actual param are the same
				if (func_data.promoted_params[i]->kind != NodeInstruction::Kind::Promoted) {
					auto assign = std::make_unique<NodeSingleAssignment>(
						formal_param->to_reference(),
						std::move(actual_param),
						node.tok
					);
					assign->kind = NodeInstruction::ParameterStack;
					assign->collect_references();
					block->add_as_stmt(std::move(assign));
				}

			}

			// now there might be nullptr in the call args. Collect the args by reference
			auto actual_params_by_ref = std::make_unique<NodeParamList>(node.tok);
			for (const int idx : func_data.reference_param_indices) {
				actual_params_by_ref->add_param(std::move(node.function->get_arg(idx)));
			}
			node.function->set_args(std::move(actual_params_by_ref));
			node.function->collect_references();
		}

		if (!block->statements.empty() || !return_block->statements.empty()) {
			swap_call(node, block);
			if (!return_block->statements.empty()) {
				block->append_body(std::move(return_block));
			}
			return node.replace_with(std::move(block));
		}

		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
		node.header ->accept(*this);
		if (node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);

		// insert all function definitions currently on the call stack to subcalls per function
		// add all definitions on the callstack to all other functions as subcalls
		// for (const auto& call : m_function_call_stack) {
		// 	auto existing_def = call->get_definition().get();
		// 	auto current_def = &node;
		// 	if (existing_def) {
		// 		m_subcalls_per_function[current_def].insert(existing_def);
		// 		m_subcalls_per_function[existing_def].insert(current_def);
		// 	}
		// }

		m_subcalls_per_function[&node].insert(&node);

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
					m_param_decl_per_function[&node].push_back(decl->variable.get());
					decl->kind = NodeInstruction::ParameterStack;
					promoted_params.push_back(std::move(param));
					promoted_param_indices.push_back(i);
					// WHY?????
					// if the call is currently in the on init callback, do not move to global_declarations but directly
					// if (m_program->current_callback == m_program->init_callback) {
					// 	m_global_declarations[m_function_call_stack[0]].push_back(std::move(decl));
					// } else {
						m_program->global_declarations->add_as_stmt(std::move(decl));
					// }

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


