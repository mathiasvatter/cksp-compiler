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
class ASTParameterPromotion final : public ASTGlobalScope {
	/// map for local variable declarations per function definition to be added to the next/above function
	std::unordered_map<NodeFunctionDefinition*, std::vector<NodeDataStructure*>> m_local_var_declarations;

	/// map for local variable declarations per function definition to be added to the next/above callback when no function is above
	std::unordered_map<NodeStatement*, std::map<std::string, std::shared_ptr<NodeDataStructure>>> m_declares_per_stmt;
	/// map for local variable declarations per function definition directly promoted and to be added to the global declarations
	std::unordered_map<std::string, std::shared_ptr<NodeDataStructure>> m_global_function_vars;

public:
	explicit ASTParameterPromotion(NodeProgram* main) : ASTGlobalScope(main) {}
	// ~ASTParameterPromotion() = default;

	void do_param_promotion(NodeProgram& program) {
		program.accept(*this);
		// insert_promoted_declarations();
		// clear all maps
		// m_declares_per_stmt.clear();
		// m_local_var_declarations.clear();
		// m_global_function_vars.clear();
	}

	void do_param_promotion(NodeFunctionCall& call) {
		m_program->current_callback = nullptr;
//		m_program->reset_function_visited_flag();
		if(auto decl = call.get_definition()) {
			decl->visited = false;
		}
		call.accept(*this);
		insert_promoted_declarations();

		// clear all maps
		m_declares_per_stmt.clear();
		m_local_var_declarations.clear();
		m_global_function_vars.clear();
	}

private:
	/// insert declarations from m_declares_per_stmt into callbacks
	/// - insert right above the function calls when param promoted
	/// - insert to global declarations when not param promoted
	void insert_promoted_declarations() {
		// successivelly promoted local vars
		for(auto & stmt : m_declares_per_stmt) {
			// if in callback, put the declarations right above the function call
			auto node_body = std::make_unique<NodeBlock>(Token());
			node_body->scope = true;
			for(auto &[fst, snd] : stmt.second) {
				// decl.second->is_promoted = true;
				node_body->add_as_stmt(
					std::make_unique<NodeSingleDeclaration>(snd, nullptr, snd->tok)
				);
			}
			node_body->add_as_stmt(std::move(stmt.first->statement));
			stmt.first->set_statement(std::move(node_body));
		}

		// directly promoted local vars
		for(auto &[fst, snd] : m_global_function_vars) {
			m_program->global_declarations->add_as_stmt(
				std::make_unique<NodeSingleDeclaration>(snd, nullptr, snd->tok)
			);
		}
	}

private:
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		return &node;
	}

	NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);

        if(node.kind == NodeFunctionCall::Kind::Property) {
            CompileError(ErrorType::InternalError,
                         "Found undefined <property function>.", "", node.tok).exit();
        } else if (node.kind == NodeFunctionCall::Kind::Builtin) {
            // no param promotion for builtin function pls
            return &node;
        }

		// only isolated functions can undergo param promotion
		if(!node.parent->cast<NodeStatement>()) {
			return &node;
		}


		bool needs_param_promotion = node.do_param_promotion();
		needs_param_promotion = true;
		// for now, when function does not need param promotion and has no params and is not in init callback, assume as call
//		if(!needs_param_promotion and node.get_definition()->has_no_params() and !m_program->is_init_callback(m_program->current_callback)) {
//			node.is_call = true;
//		}

		auto definition = node.get_definition();
		if(definition and !definition->visited) {
			definition->accept(*this);
			auto func_local_vars = std::move(definition->do_variable_reuse(m_program));


			// if(!m_local_var_declarations[definition.get()].empty()) {
				// see if this call is in thread safe env -> if not, clone and promote local vars
				if (needs_param_promotion) {

					for (auto &var : func_local_vars) {
						auto declaration = var->parent->cast<NodeSingleDeclaration>();
						if (!declaration) {
							auto error = CompileError(ErrorType::InternalError, "Variable not in declaration.", "", var->tok);
							error.exit();
						}
						auto assignment = to_assign_statement(*declaration);
						// add local declarations of function definition to parameters
						definition->header->add_param(var);
						m_local_var_declarations[definition.get()].push_back(var.get());
						declaration->replace_with(std::move(assignment));
					}

					// for (auto &decl : m_local_var_declarations[definition.get()]) {
					//
					//
					// }


				// if no param promotion, add directly to global declarations
				} else {
					// put local declaration into global declaration as a one time thing
					auto &declares = m_local_var_declarations[definition.get()];
					// check if variables were already added to global declarations
					if (!declares.empty()) {
						// otherwise add them to global declarations
						for (auto &decl : declares) {
							decl->to_global();
							// set to global to prevent from being used in other functions by register reuse
							// m_global_function_vars.emplace(decl.first, std::move(decl.second));
						}
						declares.clear();
					}
				}
			// }

		}

		if(definition) {
			// // if the call does need param promotion
			// if(needs_param_promotion) {
			// 	// if the call is in a callback -> check if threadsafe (add to global declarations) or not (do parameter promotion)
			// 	if (node.get_current_callback()) {
			// 		auto last_stmt = node.get_parent_statement();
			// 		// add declaration statements to the statement right above the function call
			// 		for (auto &decl : m_local_var_declarations[definition.get()]) {
			// 			m_declares_per_stmt[last_stmt].emplace(decl.first, clone_shared<NodeDataStructure>(decl.second));
			// 		}
			// 	}
			// 	// if the call is in a nested function -> get the local var declaration map to the above function
			// 	if(auto curr_func = node.get_current_function()) {
			// 		// add declaration statements to the body of the current/above function
			// 		auto &next_declares = m_local_var_declarations[curr_func];
			// 		for (auto &decl : m_local_var_declarations[definition.get()]) {
			// 			next_declares.emplace(decl.first, clone_shared<NodeDataStructure>(decl.second));
			// 		}
			// 	}
			// }

			// get declarations of promoted function-local variables above the function call
			auto node_body = std::make_unique<NodeBlock>(Token(), true);
			for (auto &decl : m_local_var_declarations[definition.get()]) {
				auto var = clone_as<NodeDataStructure>(decl);
				// add references to those local variables in the function call
				auto ref = var->to_reference();
				// // the ref in the call should not be connected to the param in the definition
				// ref->declaration.reset();
				node.function->add_arg(std::move(ref));
				auto promoted_decl = std::make_unique<NodeSingleDeclaration>(std::move(var), decl->tok);
				promoted_decl->kind = NodeSingleDeclaration::Kind::Promoted;
				node_body->add_as_stmt(std::move(promoted_decl));
			}
			node_body->add_as_stmt(node.clone());
			return node.replace_with(std::move(node_body));

// 			// remove call flag when function is not thread safe
// 			if(!definition->is_thread_safe and !node.function->has_no_args()) {
// 				node.is_call = false;
// 			} else if (definition->is_thread_safe and node.function->has_no_args()){
// //				if(m_program->current_callback != m_program->init_callback) node.is_call = true;
// 			}

		}
		return &node;
	}

	NodeAST* visit(NodeFunctionDefinition& node) override {
		// node.do_variable_reuse(m_program);
//		m_program->function_call_stack.push(node.weak_from_this());
		node.visited = true;
		node.body->accept(*this);
//		m_program->function_call_stack.pop();
		return &node;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) node.value->accept(*this);

		// // return if not in function
		// const auto curr_func = node.get_current_function();
		// if(!curr_func) return &node;;

        // visit declaration node because array as function param needs to have <no brackets>
        node.variable->accept(*this);

		// auto assignment = to_assign_statement(node);
		// m_local_var_declarations[curr_func].emplace(
	 //        node.variable->name,
		// 	std::move(node.variable)
  //       );
  //
		// return node.replace_with(std::move(assignment));

		return &node;
	}

    NodeAST* visit(NodeArray& node) override {
		if(node.size) node.size->accept(*this);
        node.show_brackets = false;
		return &node;
    }


};
