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
		if(node.size() == 1 and node.param(0)->cast<NodeParamList>()) {
			node.flatten();
		}

		// transformation into initializer list:
		// in case we are r_value in a declaration statement
		if(auto decl = node.parent->cast<NodeSingleDeclaration>()) {
			if(decl->value and decl->value.get() == &node) {
				return node.replace_with(node.to_initializer_list());
			}
		}

		// in case we are r_value in a assignment statement
		if(auto assign = node.parent->cast<NodeSingleAssignment>()) {
			if(assign->r_value.get() == &node) {
				return node.replace_with(node.to_initializer_list());
			}
		}

		// in case we are inside a function call
		if(node.parent->cast<NodeParamList>() and node.parent->parent->cast<NodeFunctionHeader>()) {
			return node.replace_with(node.to_initializer_list());
		}

		// in case we are in return statement
		if(node.parent->cast<NodeReturn>()) {
			return node.replace_with(node.to_initializer_list());
		}

		return &node;
	}

};

