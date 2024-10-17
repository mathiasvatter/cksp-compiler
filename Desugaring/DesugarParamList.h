//
// Created by Mathias Vatter on 17.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugars ParamLists by transforming them into Initializer Lists when the following holds:
 * 1. The ParamList is argument of a function call
 * 2. The ParamList is value in a declaration statement
 */
class DesugarParamList : public ASTDesugaring {
public:
	explicit DesugarParamList(NodeProgram* program) : ASTDesugaring(program) {};

	NodeAST * visit(NodeParamList &node) override {

		// in case it is a double param_list [[0,1,2,3]] -> flatten
		if(node.params.size() == 1 and node.params[0]->get_node_type() == NodeType::ParamList) {
			node.flatten();
		}

		// transformation into initializer list:
		// in case we are r_value in a declaration statement
		if(node.parent->get_node_type() == NodeType::SingleDeclaration) {
			auto declaration = static_cast<NodeSingleDeclaration*>(node.parent);
			if(declaration->value and declaration->value.get() == &node) {
				node.replace_with(node.to_initializer_list());
			}
		}

		// in case we are r_value in a assignment statement
		if(node.parent->get_node_type() == NodeType::SingleAssignment) {
			auto assignment = static_cast<NodeSingleAssignment*>(node.parent);
			if(assignment->r_value.get() == &node) {
				node.replace_with(node.to_initializer_list());
			}
		}

		// in case we are inside a function call
		if(node.parent->get_node_type() == NodeType::ParamList
			and node.parent->parent->get_node_type() == NodeType::FunctionHeader) {
			node.replace_with(node.to_initializer_list());
		}

		return &node;
	}

};

