//
// Created by Mathias Vatter on 21.05.24.
//

#pragma once

#include "../ASTVisitor.h"
#include "ASTVariableReuse.h"

/**
 * @brief Promotes parameters for all functions until all variables are in callbacks.
 *
 * - Visits each function call up to the last nested call, adds local declarations as new parameters,
 *   and maps pointers to the next higher function definitions.
 * - Repeats this process until the function call stack is empty and inserts the declarations into the callback.
 * - these declarations in callbacks are marked as promoted, to be removed later
 */
class ASTParameterPromotion final : public ASTVisitor {
	DefinitionProvider* m_def_provider;
	NodeCallback* m_current_callback = nullptr;
	/// map for local variable declarations per function definition to be added to the next/above function
	std::unordered_map<NodeFunctionDefinition*, std::vector<NodeDataStructure*>> m_local_var_declarations;
	std::vector<std::unique_ptr<NodeSingleDeclaration>> m_global_declarations;
public:
	explicit ASTParameterPromotion(NodeProgram* main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	void do_param_promotion(NodeProgram& program) {
		program.accept(*this);
		m_local_var_declarations.clear();
		m_global_declarations.clear();
	}

private:
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
				definition->accept(*this);
				definition->visited = true;
				const auto func_local_vars = std::move(definition->do_variable_reuse(m_program));

				bool has_param_stack = false;
				// for (const auto &call : definition->call_sites) {
				// 	call->determine_function_strategy(m_program, m_current_callback);
				// 	if (call->strategy == NodeFunctionCall::Strategy::ParameterStack or call->strategy == NodeFunctionCall::Strategy::Call) {
				// 		has_param_stack = true;
				// 		break;
				// 	}
				// }

				// promote if no param stack
				for (auto &var : func_local_vars) {
					const auto declaration = var->parent->cast<NodeSingleDeclaration>();
					if (!declaration) {
						auto error = CompileError(ErrorType::InternalError, "Variable not in declaration.", "", var->tok);
						error.exit();
					}
					auto assignment = ASTVariableReuse::to_assign_statement(*declaration);
					// if (!has_param_stack) {
					// 	// add local declarations of function definition to parameters
					// 	definition->header->add_param(var);
					// 	definition->header->params.back()->kind = NodeInstruction::Promoted;
					// 	m_local_var_declarations[definition.get()].push_back(var.get());
					// } else {
					// 	auto global_decl = std::make_unique<NodeSingleDeclaration>(var, nullptr, var->tok);
					// 	var->to_global();
					// 	m_program->global_declarations->add_as_stmt(std::move(global_decl));
					// }
					if (!var->ty->cast<CompositeType>()) {
						// add local declarations of function definition to parameters
						definition->header->add_param(var);
						definition->header->params.back()->kind = NodeInstruction::Promoted;
						m_local_var_declarations[definition.get()].push_back(var.get());
					} else {
						auto global_decl = std::make_unique<NodeSingleDeclaration>(var, nullptr, var->tok);
						var->to_global();
						// if the call is currently in the on init callback, do not move to global_declarations but directly
						// above the call (with the interim map m_global_declarations)
						if (m_current_callback == m_program->init_callback) {
							m_global_declarations.push_back(std::move(global_decl));
						} else {
							// add to global declarations
							m_program->init_callback->statements->add_as_stmt(std::move(global_decl));
						}
					}
					declaration->replace_with(std::move(assignment));
				}

			}

			// move declarations of promoted function-local variables above the function call
			auto node_body = std::make_unique<NodeBlock>(Token(), true);
			for (const auto &decl : m_local_var_declarations[definition.get()]) {
				auto var = clone_as<NodeDataStructure>(decl);
				// add references to those local variables in the function call
				auto ref = var->to_reference();
				node.function->add_arg(std::move(ref));
				auto promoted_decl = std::make_unique<NodeSingleDeclaration>(std::move(var), decl->tok);
				promoted_decl->kind = NodeSingleDeclaration::Kind::Promoted;
				node_body->add_as_stmt(std::move(promoted_decl));
			}
			// add the directly promoted global variables to node_body if not empty
			if (!m_global_declarations.empty()) {
				for (size_t i = 0; i < m_global_declarations.size(); ++i) {
					auto decl = std::move(m_global_declarations[i]);
					node_body->add_as_stmt(std::move(decl));
				}
				m_global_declarations.clear();
			}
			node_body->add_as_stmt(node.clone());
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


