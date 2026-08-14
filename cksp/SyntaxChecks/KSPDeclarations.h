//
// Created by Mathias Vatter on 19.08.24.
//

#pragma once

#include "../ASTVisitor/ASTVisitor.h"
#include "../ASTVisitor/GlobalScope/NormalizeArrayAssign.h"

/**
 * Checks that declarations with values have non-constant values.
 * if not -> split declaration into declaration and assignment statement.
 * if value is string -> split declaration into declaration and assignment statement.
 * if variable is a ui_control -> also split in any r_value case
 */
class KSPDeclarations final : public ASTVisitor {
public:

	/// Whether the array declares more elements than the list spells out. An unknown size counts
	/// as more, since the value that fills the rest has to be written either way.
	static bool leaves_elements_out(NodeArray& array, const NodeInitializerList& init_list) {
		const auto size = array.size ? array.size->cast<NodeInt>() : nullptr;
		return !size or static_cast<int32_t>(init_list.size()) < size->value;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.value) {
			if(!node.value->is_constant(false, false)
				or node.value->ty->get_element_type() == TypeRegistry::String
				or node.variable->cast<NodeUIControl>()
			) {
				auto body = std::make_unique<NodeBlock>(node.tok);
				const auto ui_control = node.variable->cast<NodeUIControl>();
				// get correct declarations and stuff
				auto new_assignment = std::make_unique<NodeSingleAssignment>(ui_control ? ui_control->control_var->to_reference() : node.variable->to_reference(), std::move(node.value), node.tok);
				auto new_declaration = std::make_unique<NodeSingleDeclaration>(node.variable, nullptr, node.tok);
				new_declaration->variable->data_type = DataType::Mutable; // all declarations without values have to be non-constant
				new_assignment->l_value->ty = new_declaration->variable->ty;

				// declare strings[4] := ("a", "b", "c", "d")
				// has to be split up because KSP stuff
				// lower initializer list when array with non-constant or string values
				if (const auto array_ref = new_assignment->l_value->cast<NodeArrayRef>()) {
					if (const auto init_list = new_assignment->r_value->cast<NodeInitializerList>()) {
						// KSP fills what the list leaves out with its last value, so the element
						// wise split has to do the same - otherwise the rest of the array stays at
						// 0, which for an array of objects is a valid object index.
						const auto declared_array = new_declaration->variable->cast<NodeArray>();
						std::unique_ptr<NodeSingleDeclaration> iterator_declaration;
						if (declared_array and leaves_elements_out(*declared_array, *init_list)
							and NormalizeArrayAssign::fills_left_out_elements(*declared_array, *init_list)) {
							// The global iterators are already declared and inlined by now, so this
							// loop brings its own - <on init> takes a declaration anywhere.
							auto iterator = ASTVisitor::get_iterator_var(
								node.tok, m_program->def_provider->get_fresh_name("_iter"));
							body = NormalizeArrayAssign::get_array_init_from_declaration(
								array_ref, init_list, declared_array->get_size(), iterator);
							iterator_declaration = std::make_unique<NodeSingleDeclaration>(iterator, nullptr, node.tok);
						} else {
							body = NormalizeArrayAssign::get_array_init_from_list(array_ref, init_list);
						}
						body->prepend_as_stmt(std::move(new_declaration));
						if (iterator_declaration) body->prepend_as_stmt(std::move(iterator_declaration));
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