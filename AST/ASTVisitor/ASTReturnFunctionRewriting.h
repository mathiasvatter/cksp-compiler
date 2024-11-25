//
// Created by Mathias Vatter on 31.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "ReturnFunctionRewriting/ReturnFunctionCallHoisting.h"
#include "ReturnFunctionRewriting/ReturnParamPromotion.h"
#include "ReturnFunctionRewriting/ReturnFunctionIsolation.h"

class ASTReturnFunctionRewriting: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
public:
	inline explicit ASTReturnFunctionRewriting(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	inline void do_rewriting(NodeProgram& node) {
		node.accept(*this);
		node.reset_function_visited_flag();

		static ReturnFunctionIsolation isolation(m_program);
		isolation.do_return_function_isolation(node);

		node.update_function_lookup();
	}

private:

	inline NodeAST* visit(NodeProgram& node) override {
		m_program = &node;

		m_program->global_declarations->accept(*this);
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}

		return &node;
	};

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		node.do_return_param_promotion();
		node.visited = true;
		node.header->accept(*this);
		if(node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		return &node;
	}

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			auto definition = node.get_definition();
			if(!definition->visited) {
				definition->accept(*this);
			}
			definition->visited = true;

		}
		return node.do_function_call_hoisting(m_program);
	}

	inline NodeAST * visit(NodeFunctionHeaderRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.get_declaration() and node.is_func_arg()) {
			if(auto def = node.get_declaration()->parent->cast<NodeFunctionDefinition>()) {
				if(!def->visited) def->accept(*this);
				def->visited = true;
			}
		}
		if(node.args) node.args->accept(*this);
		return &node;
	}

};