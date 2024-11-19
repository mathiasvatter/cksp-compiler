//
// Created by Mathias Vatter on 21.05.24.
//

#pragma once

#include "ASTGlobalScope.h"

/**
 * @brief Promotes parameters (Lambda Lifting?) for all functions until all variables are in callbacks.
 *
 * - Visits each function call up to the last nested call, adds local declarations as new parameters,
 *   and maps pointers to the next higher function definitions.
 * - Repeats this process until the function call stack is empty and inserts the declarations into the callback.
 * - these declarations in callbacks are marked as promoted, to be removed later
 */
class ASTParameterPromotion : public ASTGlobalScope {
private:
	/// map for local variable declarations per function definition to be added to the next/above function
	std::unordered_map<NodeFunctionDefinition*, std::map<std::string, std::unique_ptr<NodeSingleDeclaration>>> m_local_var_declarations;
	/// map for local variable declarations per function definition to be added to the next/above callback when no function is above
	std::unordered_map<NodeStatement*, std::map<std::string, std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_stmt;
	NodeStatement* m_last_stmt = nullptr;

	std::unordered_map<std::string, std::unique_ptr<NodeSingleDeclaration>> m_global_function_vars;

//	std::unordered_map<std::string, std::unordered_set<NodeReference*>> m_references;
//	std::unordered_map<std::string, NodeDataStructure*> m_data_structures;

public:
	explicit ASTParameterPromotion(DefinitionProvider* definition_provider) : ASTGlobalScope(definition_provider) {}
	~ASTParameterPromotion() = default;

	inline NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		for(auto& def : node.function_definitions) def->visited = false;
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		for(auto & stmt : m_declares_per_stmt) {
			// if in callback, put the declarations right above the function call
			auto node_body = std::make_unique<NodeBlock>(node.tok);
			node_body->scope = true;
			for(auto &decl : stmt.second) {
				decl.second->is_promoted = true;
				node_body->add_as_stmt(
					std::make_unique<NodeSingleDeclaration>(decl.second->variable, nullptr, node.tok)
				);
			}
			node_body->add_as_stmt(std::move(stmt.first->statement));
			stmt.first->set_statement(std::move(node_body));
		}
		for(auto & m_global_var : m_global_function_vars) {
			m_program->global_declarations->add_as_stmt(std::move(m_global_var.second));
		}

		return &node;
	}

	inline NodeAST* visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		return &node;
	}

	inline NodeAST * visit(NodeStatement& node) override {
		if(m_program->function_call_stack.empty()) m_last_stmt = &node;
		node.statement->accept(*this);
		return &node;
	}

	inline NodeAST* visit(NodeCallback& node) override {
		m_program->current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_program->current_callback = nullptr;
		return &node;
	}

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);

        if(node.kind == NodeFunctionCall::Kind::Property) {
            CompileError(ErrorType::InternalError,
                         "Found undefined <property function>.", "", node.tok).exit();
        } else if (node.kind == NodeFunctionCall::Kind::Builtin) {
            // no param promotion for builtin function pls
            return &node;
        }
		bool needs_param_promotion = node.do_param_promotion();
		// for now, when function does not need param promotion and has no params and is not in init callback, assume as call
		if(!needs_param_promotion and node.get_definition()->has_no_params() and !m_program->is_init_callback(m_program->current_callback)) {
			node.is_call = true;
		}

		auto definition = node.get_definition();
		if(definition and !definition->visited) {
			definition->accept(*this);

			if(!m_local_var_declarations[definition.get()].empty()) {
				// see if this call is in thread safe env -> if not, clone and promote local vars
				if (needs_param_promotion) {
					// do this only if current call is not threadsafe environment
					for (auto &decl : m_local_var_declarations[definition.get()]) {
						// add local declarations of function definition to parameters
						definition->header->add_param(clone_as<NodeDataStructure>(decl.second->variable.get()));
						for (auto &call_site : definition->call_sites) {
							// add references to those local variables in the function call
							auto ref = decl.second->variable->to_reference();
							// the ref in the call should not be connected to the param in the definition
							ref->declaration.reset();
							call_site->function->add_arg(std::move(ref));
						}
					}

				// if no param promotion, add directly to global declarations
				} else {
					// put local declaration into global declaration as a one time thing
					auto &declares = m_local_var_declarations[definition.get()];
					// check if variables were already added to global declarations
					if (!declares.empty()) {
						// otherwise add them to global declarations
						for (auto &decl : declares) {
							// set to global to prevent from being used in other functions by register reuse
							m_global_function_vars.emplace(decl.first, std::move(decl.second));
						}
						declares.clear();
					}
				}
			}

		}

		if(definition) {
			// if the call does need param promotion
			if(needs_param_promotion) {
				// if the call is in a callback -> check if threadsafe (add to global declarations) or not (do parameter promotion)
				if (m_program->function_call_stack.empty()) {
					// add declaration statements to the statement right above the function call
					for (auto &decl : m_local_var_declarations[definition.get()]) {
						m_declares_per_stmt[m_last_stmt].emplace(decl.first, clone_as<NodeSingleDeclaration>(decl.second.get()));
					}
				}

				// if the call is in a nested function -> get the local var declaration map to the above function
				if(!m_program->function_call_stack.empty()) {
					// add declaration statements to the body of the current/above function
					auto &next_declares = m_local_var_declarations[m_program->get_current_function().get()];
					for (auto &decl : m_local_var_declarations[definition.get()]) {
						next_declares.emplace(decl.first, clone_as<NodeSingleDeclaration>(decl.second.get()));
					}
				}
			}

			// remove call flag when function is not thread safe
			if(!definition->is_thread_safe and !node.function->has_no_args()) {
				node.is_call = false;
			} else if (definition->is_thread_safe and node.function->has_no_args()){
//				if(m_program->current_callback != m_program->init_callback) node.is_call = true;
			}

		}
		return &node;
	}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		m_program->function_call_stack.push(node.weak_from_this());
		node.visited = true;
		node.body->accept(*this);
		m_program->function_call_stack.pop();
		return &node;
	}

	inline NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) node.value->accept(*this);

		// return if not in function
		if(m_program->function_call_stack.empty()) return &node;;

		// what is this for??
//		if(node.variable->data_type == DataType::Const) {
//			m_program->global_declarations->add_stmt(std::make_unique<NodeStatement>(std::move(node.clone()), node.tok));
//			node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
//			return &node;
//		}


        // visit declaration node because array as function param needs to have <no brackets>
        node.variable->accept(*this);

		auto assignment = to_assign_statement(node);
		m_local_var_declarations[m_program->get_current_function().get()].emplace(
                        node.variable->name,
                       std::make_unique<NodeSingleDeclaration>(
                            node.variable,
                            nullptr, node.tok
                       )
                   );

		return node.replace_with(std::move(assignment));
	}

    inline NodeAST* visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
        node.show_brackets = false;
		return &node;
    }


};
