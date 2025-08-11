//
// Created by Mathias Vatter on 04.04.25.
//

#pragma once

#include "../AST/ASTVisitor/ASTKSPSyntaxCheck.h"
#include "../AST/ASTVisitor/ASTOptimizations.h"

/**
 * Tries to raise unrolled array initializations back to NodeInitializerList
 * Pre Raising
 * 	declare arr[3]: int[]
 * 	arr[0] := 1
 *	arr[1] := 5
 *	arr[2] := 3
 * Post Raising
 *  declare arr[3]: int[] := (1, 5, 3)
 */
class ArrayInitializationRaising final : public ASTOptimizations {
	// maps array name to the assignments
	std::vector<NodeSingleDeclaration*> arrays_being_tracked;
	std::unordered_map<std::string, std::vector<NodeSingleAssignment*>> array_ref_assignments;
	std::unordered_map<std::string, std::vector<std::unique_ptr<NodeAST>>> initializer_list;
	std::unordered_map<std::string, std::unique_ptr<NodeAST>> type_neutral_val;
public:
	NodeAST* do_initialization_raising(NodeCallback& node, NodeProgram* program) {
		if (&node != program->init_callback) return &node;
		m_program = program;
		clear_data_structures();
		node.accept(*this);
		do_initializer_list_assembly();
		clear_data_structures();
		return &node;
	}


private:

	void clear_data_structures() {
		arrays_being_tracked.clear();
		array_ref_assignments.clear();
		initializer_list.clear();
		type_neutral_val.clear();
	}

	void do_initializer_list_assembly() {
		for (const auto& decl : arrays_being_tracked) {
			auto& assignments = array_ref_assignments[decl->variable->name];
			if (assignments.empty()) continue;

			auto list = std::move(initializer_list[decl->variable->name]);
			const auto type_neutral_el = std::move(type_neutral_val[decl->variable->name]);
			size_t first_non_nullptr = 0;
			for (size_t i = list.size(); i-- >0; ) {
				if (auto& el = list[i]; !el and first_non_nullptr != 0) {
					el = type_neutral_el->clone();
				} else if (el and first_non_nullptr == 0) {
					first_non_nullptr = i;
				}
			}
			list.resize(first_non_nullptr+1);
			auto init_list = std::make_unique<NodeInitializerList>(decl->tok);
			init_list->elements = std::move(list);
			init_list->set_child_parents();
			decl->set_value(std::move(init_list));
			for (const auto assign : assignments) {
				assign->remove_node();
			}
		}
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		/// WHY WAS THIS HERE???
		if (node.variable->data_type == DataType::Const) {
			clear_data_structures();
			return &node;
		}
		const auto array = node.variable->cast<NodeArray>();
		if (!array) return &node;
		if (!array->size) return &node;
		// string arrays should not be raised because of KSP
		if (array->ty->get_element_type() == TypeRegistry::String) return &node;
		// array->size->accept(*this);
		array->size->do_constant_folding();
		if (const auto node_int = array->size->cast<NodeInt>()) {
			// check if size is negative due to buffer overflow
			if (node_int->value < 0) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message = "Array size cannot be negative. This is most likely due to a buffer overflow.";
				error.exit();
			}
			if (node_int->value > MAX_ARRAY_ELEMENTS) {
				return &node;
			}
			arrays_being_tracked.push_back(&node);
			array_ref_assignments[array->name].clear();
			initializer_list[array->name].resize(node_int->value);
			type_neutral_val[array->name] = TypeRegistry::get_neutral_element_from_type(array->ty->get_element_type());
			// _chord.types[34] := [-1, 34,5] the last should be applied everywhere unless it is the type neutral value
			if (node.value) {
				if (const auto init_list = node.value->cast<NodeInitializerList>()) {
					// if only one element
					if (init_list->elements.size() == 1) {
						type_neutral_val[array->name] = init_list->elements[0]->clone();
					} else if (init_list->elements.size() > 1) {
						type_neutral_val[array->name] = init_list->elements.back()->clone();
					}
					// apply to initializer_list data structure
					auto &init_list_saved = initializer_list[array->name];
					for (int i = 0; i<init_list->elements.size(); i++) {
						const auto& el = init_list->elements[i];
						if (i < init_list_saved.size()) {
							init_list_saved[i] = el->clone();
						} else {
							init_list_saved.push_back(el->clone());
						}
					}
				// } else {
				// 	auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				// 	error.m_message = "<Array> can only be assigned with a list of values.";
				// 	error.m_expected = "<InitializerList>";
				// 	error.exit();
				}
				return &node;
			}
		}
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		for(const auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment& node) override {
		if (const auto arr_ref = node.l_value->cast<NodeArrayRef>()) {
			if (!arr_ref->has_index()) return &node;
			if (!initializer_list.contains(arr_ref->name)) return &node;
			if (!node.r_value->is_constant()) return &node;
			if (node.r_value->cast<NodeFunctionCall>()) return &node;
			const auto &idx = arr_ref->index;
			// idx->accept(*this);
			idx->do_constant_folding();
			if (const auto node_int = idx->cast<NodeInt>()) {
				auto& init_list = initializer_list[arr_ref->name];
				if (init_list.size() <= node_int->value) {
					auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
					error.m_message = "Array index out of bounds. Index exceeds the array size given in its declaration: "+std::to_string(init_list.size())+".";
					error.exit();
				}
				// if this idx was not already set
				if (!init_list[node_int->value]) {
					init_list.at(node_int->value) = node.r_value->clone();
					array_ref_assignments[arr_ref->name].push_back(&node);
				}
			}

		}
		return &node;
	}


	// // check if variable reference is constant and can be substituted
	// NodeAST* visit(NodeVariableRef& node) override {
	// 	return node.try_constant_value_replace();
	// }

};
