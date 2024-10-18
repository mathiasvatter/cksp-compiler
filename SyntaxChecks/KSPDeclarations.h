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
				auto new_declaration = std::make_unique<NodeSingleDeclaration>(std::move(node.variable), nullptr, node.tok);
				auto node_assignment_ref = static_cast<NodeReference*>(new_assignment->l_value.get());
				node_assignment_ref->match_data_structure(new_declaration->variable.get());
				node_assignment_ref->ty = new_declaration->variable->ty;

				// lower initializer list when array with non-constant or string values
				if(new_declaration->variable->get_node_type() == NodeType::Array and new_assignment->r_value->get_node_type() == NodeType::InitializerList) {
					auto array_ref = static_cast<NodeArrayRef*>(node_assignment_ref);
					auto init_list = static_cast<NodeInitializerList *>(new_assignment->r_value.get());
					body = NormalizeArrayAssign::get_array_init_from_list(array_ref, init_list);
					body->prepend_stmt(std::make_unique<NodeStatement>(
							std::move(new_declaration),
						    node.tok)
						);
				} else {
					body->add_stmt(std::make_unique<NodeStatement>(
							std::move(new_declaration),
							node.tok)
						);
					body->add_stmt(std::make_unique<NodeStatement>(
							std::move(new_assignment),
							node.tok)
						);
				}
				return node.replace_with(std::move(body));
			}
		}
		return &node;
	};


};