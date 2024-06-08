//
// Created by Mathias Vatter on 21.05.24.
//

#pragma once

#include "ASTVisitor.h"

class ASTLambdaLifting : public ASTVisitor {
public:
	explicit ASTLambdaLifting(DefinitionProvider* definition_provider) : m_def_provider(definition_provider) {}
	~ASTLambdaLifting() = default;

	inline void visit(NodeProgram& node) override {
		m_program = &node;
		for(auto & def : node.function_definitions) def->visited = false;

		// first pass to analyze dynamic extend within function definitions and replace with passive_vars
		ASTGlobalScope global_scope(m_def_provider);
		global_scope.set_program_ptr(m_program);
		for (auto & def : node.function_definitions) {
			global_scope.clear_passive_vars();
			def->accept(global_scope);
		}
		global_scope.rename_local_vars();
		for(auto & def : node.function_definitions) def->visited = false;

		for(auto & callback : node.callbacks) {
			callback->accept(*this);
			// create node body with local variable declarations for every callback and prepend it to the callback
			auto node_body = std::make_unique<NodeBody>(callback->tok);
			for(auto &decl : m_declares_per_callback[callback.get()]) {
				node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(decl.second), callback->tok));
			}
			callback->statements->prepend_body(std::move(node_body));
		}
	}

	void inline visit(NodeCallback& node) override {
		m_program->current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_program->current_callback = nullptr;
	}

	void inline visit(NodeFunctionCall& node) override {
		node.function->accept(*this);

		node.get_definition(m_program);

        if(node.kind == NodeFunctionCall::Kind::Property) {
            CompileError(ErrorType::InternalError,
                         "Found undefined <property function>.", "", node.tok).exit();
        } else if (node.kind == NodeFunctionCall::Kind::Builtin) {
            // no lambda lifting for builtin function pls
            return;
        }

		if(node.definition and not node.definition->visited) {
			// extra arguments in definition
			// get declare statements of local variables

			node.definition->accept(*this);
			for (auto &decl : m_local_var_declarations[node.definition]) {
				// add local declarations of function definition to parameters
				node.definition->header->args->params.push_back(decl.second->to_be_declared->clone());
				for (auto &call_site : node.definition->call_sites) {
					// add references to those local variables in the function call
					call_site->function->args->params.push_back(decl.second->to_be_declared->to_reference());
					call_site->function->args->params.back()->parent = call_site->function->args.get();
				}
			}
			node.definition->header->args->set_child_parents();
		}

		if(node.definition) {
			// add declaration statements to the body of the current/above function or callback if function stack is empty
			auto& next_declares = m_program->function_call_stack.empty() ? m_declares_per_callback[m_program->current_callback] : m_local_var_declarations[m_program->function_call_stack.top()];
			for (auto &decl : m_local_var_declarations[node.definition]) {
				next_declares.emplace(decl.first,clone_as<NodeSingleDeclaration>(decl.second.get()));
			}

			// remove call flag when function call got parameters
			if(!node.function->args->params.empty()) {
				node.is_call = false;
			}
		}
	}

	void inline visit(NodeFunctionDefinition& node) override {
		m_program->function_call_stack.push(&node);
		node.body->accept(*this);
		node.visited = true;
		m_program->function_call_stack.pop();
	}

	void inline visit(NodeSingleDeclaration& node) override {
		if(node.assignee) node.assignee->accept(*this);

		// return if not in function
		if(m_program->function_call_stack.empty()) return;
        // visit declaration node because array as function param needs to have <no brackets>
        node.to_be_declared->accept(*this);
		m_local_var_declarations[m_program->function_call_stack.top()].emplace(
                        node.to_be_declared->name,
                       std::make_unique<NodeSingleDeclaration>(
                            clone_as<NodeDataStructure>(node.to_be_declared.get()),
                            nullptr, node.tok
                       )
                   );
		node.replace_with(node.to_assign_stmt());
	}

    void inline visit(NodeArray& node) override {
        node.size = nullptr;
        node.show_brackets = false;
    }


private:
	DefinitionProvider* m_def_provider;
	/// map for local variable declarations per function definition to be added to the next/above function
	std::unordered_map<NodeFunctionDefinition*, std::map<std::string, std::unique_ptr<NodeSingleDeclaration>>> m_local_var_declarations;
	/// map for local variable declarations per function definition to be added to the next/above callback when no function is above
	std::unordered_map<NodeCallback*, std::map<std::string, std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_callback;

};
