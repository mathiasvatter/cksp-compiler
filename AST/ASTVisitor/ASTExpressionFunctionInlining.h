//
// Created by Mathias Vatter on 09.08.24.
//

#pragma once


#include "ASTFunctionInlining.h"

/// Immediate Inlining of return-only functions
/// Substitution already takes place in the parent class ASTFunctionInlining in the following nodes:
/// NodeArrayRef
/// NodeNDArrayRef
/// NodeVariableRef
/// NodeFunctionHeader
class ASTExpressionFunctionInlining: public ASTFunctionInlining {
private:
public:
	inline explicit ASTExpressionFunctionInlining(DefinitionProvider *definition_provider) : ASTFunctionInlining(definition_provider) {}

	NodeAST* visit(NodeProgram &node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);
		if(node.kind != NodeFunctionCall::Kind::UserDefined) return &node;

		if(node.get_definition(m_program)) {
			// see if the function is a return-only function
			if(is_expression_function(node.definition)) {
				check_recursion(node.definition);
				m_program->function_call_stack.push(node.definition);
				m_functions_in_use.insert(node.definition);

				auto node_func_def = clone_as<NodeFunctionDefinition>(node.definition);
				m_substitution_stack.push(get_substitution_map(node_func_def->header.get(), node.function.get()));

				node_func_def->body->accept(*this);

				m_substitution_stack.pop();
				m_functions_in_use.erase(node.definition);
				m_program->function_call_stack.pop();

				return node.replace_with(get_expression_func_return(node_func_def.get()));
			}

		}
		return &node;
	}

	static inline bool is_expression_function(NodeFunctionDefinition* def) {
		if(def->num_return_params > 0) {
			if(def->body->statements.size() == 1) {
				auto stmt = def->body->statements[0]->statement.get();
				if(stmt->get_node_type() == NodeType::Return) {
					return true;
				}
			}
		}
		return false;
	}

	static inline std::unique_ptr<NodeAST> get_expression_func_return(NodeFunctionDefinition* def) {
		auto stmt = def->body->statements[0]->statement.get();
		if(stmt->get_node_type() == NodeType::Return) {
			auto ret = static_cast<NodeReturn*>(stmt);
			return std::move(ret->return_variables[0]);
		}
		auto error = CompileError(ErrorType::InternalError, "", "", def->tok);
		error.m_message = "Function is not a return-only function";
		error.exit();
		return nullptr;
	}


};