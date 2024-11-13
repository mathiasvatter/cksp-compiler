//
// Created by Mathias Vatter on 09.08.24.
//

#pragma once


#include "ASTFunctionInlining.h"
#include "ASTReturnParamPromotion.h"

/// Immediate Inlining of functions with Initializer Lists as arguments so to bypass needing to declare throwaway arrays
class ASTInitializerFunctionInlining: public ASTFunctionInlining {
private:
public:
	inline explicit ASTInitializerFunctionInlining(DefinitionProvider *definition_provider) : ASTFunctionInlining(definition_provider) {}

	NodeAST* visit(NodeProgram &node) override {
		node.reset_function_used_flag();
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();

		return &node;
	}

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		if(node.get_definition(m_program)) {
			if(node.is_builtin_kind()) return &node;
			// see if the function has initializer list arguments
			if(is_initializer_function(node)) {

				m_program->function_call_stack.push(node.definition);

				auto node_func_body = clone_as<NodeBlock>(node.definition->body.get());
				m_substitution_stack.push(get_substitution_map(node.definition->header.get(), node.function.get()));
				node_func_body->accept(*this);

				m_substitution_stack.pop();
				m_program->function_call_stack.pop();
				return node.replace_with(std::move(node_func_body));
			}
		}
		return &node;
	}

	NodeAST * visit(NodeFunctionHeaderRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.get_declaration() and node.is_func_arg()) {
			if(auto def = node.get_declaration()->parent->cast<NodeFunctionDefinition>()) {
				def->is_used = true;
				def->accept(*this);
				def->visited = true;
			}
		}
		if(node.args) node.args->accept(*this);
		return &node;
	}

	static inline bool is_initializer_function(const NodeFunctionCall& call) {
		for(auto &arg : call.function->args->params) {
			if(arg->get_node_type() == NodeType::InitializerList) {
				return true;
			}
		}
		return false;
	}


};