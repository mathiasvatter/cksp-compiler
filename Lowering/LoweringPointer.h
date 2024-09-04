//
// Created by Mathias Vatter on 07.07.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringPointer : public ASTLowering {
private:

public:
	explicit LoweringPointer(NodeProgram *program) : ASTLowering(program) {}

//	inline void visit(NodeSingleDeclaration& node) override {
//		// if declaration is pointer -> always initialize with nil!
//		if(node.variable->ty->get_element_type()->get_type_kind() == TypeKind::Object) {
//			if(!node.value) {
//				auto nil = std::make_unique<NodeNil>(node.tok);
//				nil->parent = &node;
//				if(node.variable->get_node_type() == NodeType::Variable) {
//					node.value = std::move(nil);
//					// pack into param list if declaration is array type
//				} else {
//					auto param_list = std::make_unique<NodeParamList>(node.tok, std::move(nil));
//					param_list->parent = &node;
//					node.value = std::move(param_list);
//				}
//			}
//		}
//	}

	inline NodeAST * visit(NodePointer& node) override {
		auto node_var = node.to_variable();
		node_var->ty = node.ty;
		return node.replace_with(std::move(node_var));
//		node.lower_type();
	}

	inline NodeAST * visit(NodePointerRef& node) override {
		// check if parent string -> call __repr__ method
		if(node.ty == TypeRegistry::Nil) {
			auto new_node = node.replace_with(std::make_unique<NodeNil>(node.tok));
			if(auto lowering = new_node->get_lowering(m_program)) {
				return new_node->accept(*lowering);
			}
			return new_node;
		}
		if(node.is_string_repr()) {
			return node.replace_with(node.get_repr_call());
		}
		return &node;
	}
};