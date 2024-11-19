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
		node.reset_function_used_flag();
		node.reset_function_visited_flag();
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		node.remove_unused_functions();
		return &node;
	}

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		if(node.bind_definition(m_program)) {
			if(node.is_builtin_kind()) return &node;
			auto definition = node.get_definition();
			if(!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;
			// see if the function is a return-only function
			if(definition->is_expression_function()) {

				m_program->function_call_stack.push(definition);
				auto node_func_body = clone_as<NodeBlock>(definition->body.get());
				m_substitution_stack.push(get_substitution_map(definition->header.get(), node.function.get()));
				node_func_body->accept(*this);

				m_substitution_stack.pop();
				m_program->function_call_stack.pop();
				return node.replace_with(get_expression_return(node_func_body.get()));
			} else {
				definition->is_used = true;
			}
		}
		return &node;
	}

	NodeAST * visit(NodeFunctionHeaderRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.get_declaration() and node.is_func_arg()) {
			if(node.get_declaration()->parent->get_node_type() == NodeType::FunctionDefinition) {
				auto def = static_cast<NodeFunctionDefinition*>(node.get_declaration()->parent);
				def->is_used = true;
				def->accept(*this);
				def->visited = true;
			}
		}
		if(node.args) node.args->accept(*this);
		return &node;
	}

	static inline std::unique_ptr<NodeAST> get_expression_return(NodeBlock* body) {
		auto stmt = body->statements[0]->statement.get();
		if(stmt->get_node_type() == NodeType::Return) {
			auto ret = static_cast<NodeReturn*>(stmt);
			return std::move(ret->return_variables[0]);
		}
		auto error = CompileError(ErrorType::InternalError, "", "", body->tok);
		error.m_message = "Function is not a return-only function";
		error.exit();
		return nullptr;
	}


};