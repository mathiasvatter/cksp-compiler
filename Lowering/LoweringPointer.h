//
// Created by Mathias Vatter on 07.07.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringPointer : public ASTLowering {
private:

public:
	explicit LoweringPointer(NodeProgram *program) : ASTLowering(program) {}


	inline NodeAST * visit(NodePointer& node) override {
		auto node_var = node.to_variable();
		node_var->ty = node.ty;
		return node.replace_datastruct(std::move(node_var));
	}

	inline NodeAST * visit(NodePointerRef& node) override {
		// check if parent string -> call __repr__ method
		if(node.ty == TypeRegistry::Nil) {
			auto new_node = node.replace_with(std::make_unique<NodeNil>(node.tok));
			new_node->lower(m_program);
			return new_node;
		}
		auto node_var_ref = node.to_variable_ref();
		node_var_ref->ty = node.ty;
		return node.replace_reference(std::move(node_var_ref));
	}
};