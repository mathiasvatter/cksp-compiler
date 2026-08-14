//
// Created by Mathias Vatter on 21.11.24.
//

#pragma once

#include "../ASTVisitor.h"

class FunctionDefinitionOrdering : public ASTVisitor {
private:
//	DefinitionProvider* m_def_provider = nullptr;
	std::vector<std::shared_ptr<NodeFunctionDefinition>> m_ordered_function_definitions;
public:
//	explicit FunctionDefinitionOrdering(NodeProgram *main) : m_def_provider(main->def_provider) {
//		m_program = main;
//	}

	void order_functions(NodeProgram& node) {
		m_program = &node;
		m_ordered_function_definitions = {};
		node.reset_function_visited_flag();
		node.reset_function_used_flag();

		node.global_declarations->accept(*this);
		node.init_callback->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			if(callback.get() != node.init_callback) callback->accept(*this);
		}

		node.function_definitions = std::move(m_ordered_function_definitions);
		// Hand the definitions over rather than keeping a second set of owners. The caller holds
		// this visitor in a function-local static, so anything left here outlives main() and is
		// destroyed during __cxa_finalize, after the program it belongs to is gone: the calls in
		// a body being torn down then reach through <NodeFunctionCall::~NodeFunctionCall> into
		// definitions that are already rubble and throw out of a destructor.
		m_ordered_function_definitions.clear();
		node.reset_function_visited_flag();
	}

private:

	/// visit every function call
	NodeAST *visit(NodeFunctionCall &node) override {
		// visit header
		node.function->accept(*this);

		node.bind_definition(m_program, true);

		if(node.kind == NodeFunctionCall::Kind::Property) {
			Diagnostic(ErrorType::InternalError,"Found undefined property function.", "", node.tok).exit();
		}

		if(node.kind == NodeFunctionCall::Kind::Builtin) return &node;

		auto definition = node.get_definition();
		if(!definition->visited) {
			definition->accept(*this);
			definition->visited = true;
			definition->is_used = true;
			m_ordered_function_definitions.push_back(definition);
		}

		return &node;
	}


};

