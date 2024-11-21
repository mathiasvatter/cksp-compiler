//
// Created by Mathias Vatter on 19.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../AST/ASTVisitor/GlobalScope/NormalizeArrayAssign.h"

/**
 * Checks that declarations with values have non-constant values.
 * if not -> split declaration into declaration and assignment statement.
 * if value is string -> split declaration into declaration and assignment statement.
 */
class KSPDeclarations: public ASTVisitor {
public:

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) {
			if(!node.value->is_constant() or node.value->ty == TypeRegistry::String) {
				auto body = std::make_unique<NodeBlock>(node.tok);

				// get correct declarations and stuff
				auto new_assignment = std::make_unique<NodeSingleAssignment>(node.variable->to_reference(), std::move(node.value), node.tok);
				auto new_declaration = std::make_unique<NodeSingleDeclaration>(node.variable, nullptr, node.tok);
//				auto node_assignment_ref = static_cast<NodeReference*>(new_assignment->l_value.get());
//				node_assignment_ref->match_data_structure(new_declaration->variable.get());
				new_assignment->l_value->ty = new_declaration->variable->ty;

				// lower initializer list when array with non-constant or string values
				if(new_declaration->variable->get_node_type() == NodeType::Array and new_assignment->r_value->get_node_type() == NodeType::InitializerList) {
					auto array_ref = static_cast<NodeArrayRef*>(new_assignment->l_value.get());
					auto init_list = static_cast<NodeInitializerList *>(new_assignment->r_value.get());
					body = NormalizeArrayAssign::get_array_init_from_list(array_ref, init_list);
					body->prepend_as_stmt(std::move(new_declaration));
				} else {
					body->add_as_stmt(std::move(new_declaration));
					body->add_as_stmt(std::move(new_assignment));
				}
				return node.replace_with(std::move(body));
			}
		}
		return &node;
	};


};