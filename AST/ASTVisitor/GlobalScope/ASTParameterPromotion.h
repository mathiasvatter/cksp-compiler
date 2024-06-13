//
// Created by Mathias Vatter on 21.05.24.
//

#pragma once

#include "ASTGlobalScope.h"

class ASTParameterPromotion : public ASTGlobalScope {
private:
	/// map for local variable declarations per function definition to be added to the next/above function
	std::unordered_map<NodeFunctionDefinition*, std::map<std::string, std::unique_ptr<NodeSingleDeclaration>>> m_local_var_declarations;
	/// map for local variable declarations per function definition to be added to the next/above callback when no function is above
	std::unordered_map<NodeCallback*, std::map<std::string, std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_callback;


public:
	explicit ASTParameterPromotion(DefinitionProvider* definition_provider) : ASTGlobalScope(definition_provider) {}
	~ASTParameterPromotion() = default;

	inline void visit(NodeProgram& node) override {
		m_program = &node;
		for(auto& def : node.function_definitions) def->visited = false;
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
	}

	void inline visit(NodeBody& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
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
				node.definition->header->args->params.push_back(decl.second->variable->clone());
				for (auto &call_site : node.definition->call_sites) {
					// add references to those local variables in the function call
					call_site->function->args->params.push_back(decl.second->variable->to_reference());
					call_site->function->args->params.back()->parent = call_site->function->args.get();
				}
			}
			node.definition->header->args->set_child_parents();
		}

		// do declaration insertion only if there are local declarations in function
		if(m_local_var_declarations[node.definition].empty()) return;

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

		if(m_program->function_call_stack.empty() and !m_declares_per_callback[m_program->current_callback].empty()) {
			// if in callback, put the declarations right above the function call
			auto node_body = std::make_unique<NodeBody>(node.tok);
			node_body->scope = true;
			for(auto &decl : m_declares_per_callback[m_program->current_callback]) {
				decl.second->is_promoted = true;
				node_body->add_stmt(std::make_unique<NodeStatement>(clone_as<NodeSingleDeclaration>(decl.second.get()), node.tok));
			}
			m_declares_per_callback[m_program->current_callback].clear();
			node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node.clone()), node.tok));
			node.replace_with(std::move(node_body));
			return;
		}
	}

	void inline visit(NodeFunctionDefinition& node) override {
		m_program->function_call_stack.push(&node);
		node.body->accept(*this);
		node.visited = true;
		m_program->function_call_stack.pop();
	}

	void inline visit(NodeSingleDeclaration& node) override {
		if(node.value) node.value->accept(*this);

		// return if not in function
		if(m_program->function_call_stack.empty()) return;
		if(node.variable->data_type == DataType::Const) {
			m_program->global_declarations->add_stmt(std::make_unique<NodeStatement>(std::move(node.clone()), node.tok));
			node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
			return;
		}

        // visit declaration node because array as function param needs to have <no brackets>
        node.variable->accept(*this);
		m_local_var_declarations[m_program->function_call_stack.top()].emplace(
                        node.variable->name,
                       std::make_unique<NodeSingleDeclaration>(
                            clone_as<NodeDataStructure>(node.variable.get()),
                            nullptr, node.tok
                       )
                   );


//		auto node_assignment = node.to_assign_stmt();
//		node.replace_with(std::move(node_assignment));
		node.replace_with(to_assign_statement(node));

	}

    void inline visit(NodeArray& node) override {
//        node.size = nullptr;
        node.show_brackets = false;
    }

};
