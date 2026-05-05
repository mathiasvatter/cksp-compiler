//
// Created by Mathias Vatter on 21.05.24.
//

#pragma once

#include "../ASTVisitor.h"
#include "ASTVariableReuse.h"
#include "../../../Optimization/VariableCollector.h"
#include "../FunctionHandling/ParameterAssignmentTransformation.h"

/**
 * @brief Promotes parameters for all functions until all variables are in callbacks.
 *
 * - Visits each function call up to the last nested call,
 * - calls variable reuse
 * - adds local declarations as new parameters, and maps pointers to the next higher function definitions.
 * - Repeats this process until the function call stack is empty and inserts the declarations into the callback.
 * - these declarations in callbacks are marked as promoted, to be removed later
 * --> important: NOT promoted: Composite Types and thread-unsafe variables. Those are directly moved to the global scope
 */
class ASTParameterPromotion final : public ASTVisitor {
	DefinitionProvider* m_def_provider;
	NodeCallback* m_current_callback = nullptr;
	/// map for local variable declarations per function definition to be added to the next/above function
	std::unordered_map<NodeFunctionDefinition*, std::vector<NodeDataStructure*>> m_local_var_declarations;
	std::unordered_map<NodeFunctionCall*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_global_declarations;
	std::vector<NodeFunctionCall*> m_function_call_stack;


public:
	explicit ASTParameterPromotion(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	void do_param_promotion(NodeProgram& program) {
		m_current_callback = nullptr;
		program.accept(*this);
		m_local_var_declarations.clear();
		m_global_declarations.clear();
	}

private:

	static bool has_local_var_as_size(NodeAST& size) {
		static VariableCollector var_collector;
		var_collector.collect(size);
		return var_collector.contains_local_references();
	}

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeCallback& node) override {
		m_current_callback = &node;
		if (node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);
		m_current_callback = nullptr;
		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);

        if(node.kind == NodeFunctionCall::Kind::Property) {
            CompileError(ErrorType::InternalError, "Found undefined <property function>.", "", node.tok).exit();
        } else if (node.kind == NodeFunctionCall::Kind::Builtin) {
            // no param promotion for builtin function pls
            return &node;
        }

		// only isolated functions can undergo param promotion
		if(!node.parent->cast<NodeStatement>()) {
			return &node;
		}

		const auto definition = node.get_definition();
		if(definition) {
			// only visit definition if not already visited
			if (!definition->visited) {
				m_function_call_stack.push_back(&node);
				definition->accept(*this);
				definition->visited = true;
				const auto& func_local_vars = definition->do_variable_reuse(m_program);

				// promote if no composite type
				for (auto &var : func_local_vars) {
					if (var->data_type == DataType::Const) continue; // ignore const vars, they got already promoted to global
					// declarations by variable reuse
					const auto declaration = var->parent->cast<NodeSingleDeclaration>();
					if (!declaration) {
						auto error = CompileError(ErrorType::InternalError, "Variable not in declaration.", "", var->tok);
						error.exit();
					}
					auto assignment = ASTVariableReuse::to_assign_statement(*declaration);
					auto node_array = var->cast<NodeArray>();
					auto node_ndarray = var->cast<NodeNDArray>();
					NodeAST* size = nullptr;
					if (node_array) size = node_array->size.get();
					else if (node_ndarray) size = node_ndarray->sizes.get();
					if (!size and var->is_thread_safe) {
						// add local declarations of function definition to parameters
						definition->header->add_param(var);
						definition->header->params.back()->kind = NodeInstruction::Promoted;
						m_local_var_declarations[definition.get()].push_back(var.get());
						declaration->replace_with(std::move(assignment));

					} else if (size and has_local_var_as_size(*size)) {
						// if local function array then leave it there but make function preemptiveInlining
						// because in case size of array is depended on a param!!
						// and if we move it, the size declaration will be nullptr
						definition->has_local_dynamic_arrays = true;
					} else {
						auto global_decl = std::make_unique<NodeSingleDeclaration>(var, nullptr, var->tok);
						var->to_global();
						// if the call is currently in the on init callback, do not move to global_declarations but directly
						// above the call (with the interim map m_global_declarations)
						if (m_current_callback == m_program->init_callback) {
							m_global_declarations[m_function_call_stack[0]].push_back(std::move(global_decl));
						} else {
							// add to global declarations
							m_program->init_callback->statements->add_as_stmt(std::move(global_decl));
						}
						declaration->replace_with(std::move(assignment));
					}

				}
				m_function_call_stack.pop_back();
			}

			auto node_global_decl_body = std::make_unique<NodeBlock>(Token(), false);
			// add global declaration if available
			auto it = m_global_declarations.find(&node);
			if (it != m_global_declarations.end()) {
				for (auto & decl : it->second) {
					node_global_decl_body->add_as_stmt(std::move(decl));
				}
				m_global_declarations.erase(it);
			}

			// move declarations of promoted function-local variables above the function call
			auto node_body = std::make_unique<NodeBlock>(Token(), true);
			for (const auto &decl : m_local_var_declarations[definition.get()]) {
				auto var = clone_as<NodeDataStructure>(decl);
				// var->name = m_def_provider->get_fresh_name(var->name);
				var->clear_references();
				auto promoted_decl = std::make_unique<NodeSingleDeclaration>(std::move(var), decl->tok);
				promoted_decl->kind = NodeSingleDeclaration::Kind::Promoted;
				// add references to those local variables in the function call
				auto ref = promoted_decl->variable->to_reference();
				node.function->add_arg(std::move(ref));
				node.function->args->params.back()->collect_references();
				node_body->add_as_stmt(std::move(promoted_decl));
			}
			ParameterAssignmentTransformation::swap_call(node, node_body);

			// add the body to the global_decl_body (wo scope) if there are things to globally declare and we are in the on init
			if (!node_global_decl_body->statements.empty()) {
				node_global_decl_body->add_as_stmt(std::move(node_body));
				return node.replace_with(std::move(node_global_decl_body));
			}

			// node_body->add_as_stmt(node.clone());
			return node.replace_with(std::move(node_body));
		}

		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
		node.visited = true;
		node.body->accept(*this);
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) node.value->accept(*this);
        // visit declaration node because array as function param needs to have <no brackets>
        node.variable->accept(*this);
		return &node;
	}

    NodeAST* visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
        node.show_brackets = false;
		return &node;
    }

};

