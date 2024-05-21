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
		for(auto & def : node.function_definitions) {
			def->visited = false;
			m_function_lookup.insert({{def->header->name, (int)def->header->args->params.size()}, def.get()});
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
			auto node_body = std::make_unique<NodeBody>(callback->tok);
			for(auto &decl : m_declares_per_callback[callback.get()]) {
				if(decl) node_body->statements.push_back(std::make_unique<NodeStatement>(std::move(decl), callback->tok));
			}
			callback->statements->prepend_body(std::move(node_body));
		}


	}

	void inline visit(NodeCallback& node) override {
		m_current_callback = &node;

		if(node.callback_id) node.callback_id->accept(*this);
		node.statements->accept(*this);

		m_current_callback = nullptr;
	}

	void inline visit(NodeFunctionCall& node) override {
		node.function->accept(*this);

		if(node.definition and not node.definition->visited) {
			// extra arguments in definition
			// get declare statements of local variables

			node.definition->accept(*this);
			for (auto &declaration : m_local_var_declarations[node.definition]) {
				// add local declarations of function definition to parameters
				node.definition->header->args->params.push_back(declaration->to_be_declared->clone());
				for (auto &call_site : node.definition->call_sites) {
					// add references to those local variables in the function call
					call_site->function->args->params.push_back(declaration->to_be_declared->to_reference());
					call_site->function->args->params.back()->parent = call_site->function->args.get();
				}

			}
			node.definition->header->args->set_child_parents();
		}
		if(node.definition) {
			// add declaration statements to the body of the current function or callback if function stack is empty
			if(m_function_call_stack.empty()) {
				m_declares_per_callback[m_current_callback].insert(
					m_declares_per_callback[m_current_callback].begin(),
					std::make_move_iterator(m_local_var_declarations[node.definition].begin()),
					std::make_move_iterator(m_local_var_declarations[node.definition].end())
				);

			} else {
				m_local_var_declarations[m_function_call_stack.top()].insert(
					m_local_var_declarations[m_function_call_stack.top()].begin(),
				 	std::make_move_iterator(m_local_var_declarations[node.definition].begin()),
				 	std::make_move_iterator(m_local_var_declarations[node.definition].end())
				 );
			}

		}
	}

	void inline visit(NodeFunctionDefinition& node) override {
		m_function_call_stack.push(&node);
		node.body->accept(*this);
		node.visited = true;
		m_function_call_stack.pop();
	}

	void inline visit(NodeSingleDeclareStatement& node) override {
		if(node.assignee) node.assignee->accept(*this);

		if(m_function_call_stack.empty()) return;
		m_local_var_declarations[m_function_call_stack.top()].push_back(clone_as<NodeSingleDeclareStatement>(&node));
		node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
	}


private:
	DefinitionProvider* m_def_provider;
	NodeProgram* m_program = nullptr;
	NodeCallback* m_current_callback = nullptr;
	std::stack<NodeFunctionDefinition*> m_function_call_stack;
	std::unordered_map<StringIntKey, NodeFunctionDefinition*, StringIntKeyHash> m_function_lookup;
	std::unordered_map<NodeFunctionDefinition*, std::vector<std::unique_ptr<NodeSingleDeclareStatement>>> m_local_var_declarations;

	std::unordered_map<NodeCallback*, std::vector<std::unique_ptr<NodeSingleDeclareStatement>>> m_declares_per_callback;
};