//
// Created by Mathias Vatter on 04.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugaring of Function calls with multiple arguments that are not in assignment/declaration context
 * -> only use first return and fill up with throwaway args
 */
class DesugarFunctionCall : public ASTDesugaring {
public:
	explicit DesugarFunctionCall(NodeProgram *program) : ASTDesugaring(program) {};

	inline NodeAST * visit(NodeFunctionCall& node) override {
		node.get_definition(m_program);
		if(!node.definition) return &node;
		if(node.definition->num_return_params <= 1) return &node;
		if(node.parent->get_node_type() == NodeType::SingleDeclaration or node.parent->get_node_type() == NodeType::SingleAssignment) {
			return &node;
		}

		auto num_throwaway_args = node.definition->num_return_params - 1;
		for(int i = 0; i < num_throwaway_args; i++) {
			auto throwaway_ref = std::make_unique<NodeVariableRef>("_", node.tok);
			throwaway_ref->kind = NodeVariableRef::Kind::Throwaway;
			node.function->prepend_arg(std::move(throwaway_ref));
		}

		return &node;
	}
};