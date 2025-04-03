//
// Created by Mathias Vatter on 10.06.24.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * @class NormalizeArrayAssign2
 * Different version for NormalizeArrayAssign2
 * This class should be used when global scope is already enforced
 * Transforms assignments in the following way:
 * - if array ref is assigned an initializer list -> do for loop when one element -> hardcode assignments when multiple
 * - if array ref is assigned another array ref -> do copying process with for loops
 */
class NormalizeArrayAssign2 final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;

public:
	explicit NormalizeArrayAssign2(NodeProgram* program) {
		m_program = program;
		m_def_provider = m_program->def_provider;
	}
private:
	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		node.reset_function_visited_flag();
		m_program->global_declarations->accept(*this);
		for (const auto &callback : node.callbacks) {
			callback->accept(*this);
		}
		node.merge_function_definitions();
		return &node;
	}

	NodeAST* visit(NodeSingleAssignment& node) override {
		if(auto node_array_ref = node.l_value->cast<NodeArrayRef>()) {
			// error handling
			// if lhs is arrayref and has no index, check if array is initialized with a list of values or array copy
			if(!node_array_ref->index and not(node.r_value->cast<NodeInitializerList>() or node.r_value->cast<NodeArrayRef>())) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "<Array> can only be assigned with a list of values.";
				error.m_expected = "<InitializerList>";
				error.m_got = node.r_value->get_string();
				error.exit();
			} else if (node_array_ref->index and node.r_value->cast<NodeInitializerList>()) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Array element can only be assigned with a single value.";
				error.m_expected = "<Variable>";
				error.m_got = "<InitializerList>";
				error.exit();
			} else if (node_array_ref->index) {
				return &node;
			}

            // array[] := (1) -> for loop
			// array[] := (1,2,3,4) -> series of single index assignments
			if(auto init_list = node.r_value->cast<NodeInitializerList>()) {
				// if param list has only one value:
				if (init_list->size() == 1) {
					auto lower_bound = std::make_unique<NodeInt>(0, node.tok);
					auto upper_bound = node_array_ref->get_size();
					node_array_ref->set_index(m_program->global_iterator->to_reference());
					auto loop_assignment = std::make_unique<NodeSingleAssignment>(
						std::move(node.l_value),
						std::move(init_list->elem(0)),
						node.tok
					);
					auto block = std::make_unique<NodeBlock>(node.tok);
					block->add_as_stmt(std::move(loop_assignment));
					block->wrap_in_loop(m_program->global_iterator, std::move(lower_bound), std::move(upper_bound), false);
					return node.replace_with(std::move(block));
				}
				// series of single index assignments
				return node.replace_with(get_array_init_from_list(node_array_ref, init_list));
			}

			// array[] := array2[] -> copy assignment
			if(auto node_val_array_ref = node.r_value->cast<NodeArrayRef>()) {
				auto lower_bound = std::make_unique<NodeInt>(0, node.tok);
				auto upper_bound = node_val_array_ref->get_size();
				node_val_array_ref->set_index(m_program->global_iterator->to_reference());
				node_array_ref->set_index(m_program->global_iterator->to_reference());
				auto loop_assignment = std::make_unique<NodeSingleAssignment>(
					std::move(node.l_value),
					std::move(node.r_value),
					node.tok
				);
				auto block = std::make_unique<NodeBlock>(node.tok);
				block->add_as_stmt(std::move(loop_assignment));
				block->wrap_in_loop(m_program->global_iterator, std::move(lower_bound), std::move(upper_bound), false);
				return node.replace_with(std::move(block));

			}
		}
		return &node;
	}



public:
	static std::unique_ptr<NodeBlock> get_array_init_from_list(NodeArrayRef* array_ref, NodeInitializerList* init_list) {
		auto node_body = std::make_unique<NodeBlock>(array_ref->tok);
		for(int i = 0; i<init_list->size(); i++) {
			array_ref->index = std::make_unique<NodeInt>(i, array_ref->tok);
			const auto &elem = init_list->elem(i);
			auto node_assign = std::make_unique<NodeSingleAssignment>(
				clone_as<NodeReference>(array_ref),
				elem->clone(),
				elem->tok
			);
			node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assign), elem->tok));
		}
		return node_body;
	}

};