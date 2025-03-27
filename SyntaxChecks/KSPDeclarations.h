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
class KSPDeclarations final : public ASTVisitor {
public:

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) {
			if(!node.value->is_constant() or node.value->ty->get_element_type() == TypeRegistry::String) {
				auto body = std::make_unique<NodeBlock>(node.tok);

				// get correct declarations and stuff
				auto new_assignment = std::make_unique<NodeSingleAssignment>(node.variable->to_reference(), std::move(node.value), node.tok);
				auto new_declaration = std::make_unique<NodeSingleDeclaration>(node.variable, nullptr, node.tok);
				new_assignment->l_value->ty = new_declaration->variable->ty;

				// declare strings[4] := ("a", "b", "c", "d")
				// has to be split up because KSP stuff
				// lower initializer list when array with non-constant or string values
				if (const auto array_ref = new_assignment->l_value->cast<NodeArrayRef>()) {
					if (const auto init_list = new_assignment->r_value->cast<NodeInitializerList>()) {
						body = NormalizeArrayAssign::get_array_init_from_list(array_ref, init_list);
						body->prepend_as_stmt(std::move(new_declaration));
						return node.replace_with(std::move(body));
					}
				} else {
					body->add_as_stmt(std::move(new_declaration));
					body->add_as_stmt(std::move(new_assignment));
					return node.replace_with(std::move(body));
				}
			}
		}
		return &node;
	}


};