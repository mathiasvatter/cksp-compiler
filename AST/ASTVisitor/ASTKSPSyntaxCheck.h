//
// Created by Mathias Vatter on 18.08.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../SyntaxChecks/KSPPersistency.h"
#include "../../SyntaxChecks/KSPDeclarations.h"
#include "../../Optimization/MemoryExhaustedNesting.h"

/**
 * Adds persistence functions to variables that are declared with a persistence keyword.
 * Is assigned value to a declaration statement is not constant, split the declaration into a declaration and an assignment statement.
 * CAREFUL: This visitor will mess up the declaration pointers of the references.
 * Does the following ksp vanilla specific syntax transformations and checks:
 * - Adds persistence functions to variables that are declared with a persistence keyword.
 * - Splits declaration and assignment statements if the assignment is not constant.
 * - Checks if the maximum number of UI controls is exceeded.
 * - Checks if the maximum number of array elements is exceeded.
 * - Checks if the maximum number of statements in one block is exceeded (memory exhaustion error), caused
 * 	 by the Bison parser stackoverflow -> nest block further to avoid this error
 */
class ASTKSPSyntaxCheck final : public ASTVisitor {
	DefinitionProvider *m_def_provider;
	std::unordered_map<std::string, int> m_ui_control_count;

	static const int MAX_UI_CONTROLS = 999;
	static const int MAX_ARRAY_ELEMENTS = 1000000;

	static void check_max_array_size(NodeArray* node) {
		if(node->size->get_node_type() == NodeType::Int) {
			auto node_int = static_cast<NodeInt*>(node->size.get());
			if(node_int->value > MAX_ARRAY_ELEMENTS) {
				auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *node->size);
				error.m_message = "Array size exceeds maximum of " + std::to_string(MAX_ARRAY_ELEMENTS) + ".";
				error.exit();
			}
		}
	}

	void check_max_ui_controls(NodeUIControl* node) {
		if(m_ui_control_count[node->ui_control_type] > MAX_UI_CONTROLS) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *node);
			error.m_message = "Maximum number of UI controls exceeded for <"+node->ui_control_type+">. Counted "+std::to_string(m_ui_control_count[node->ui_control_type])+" controls.";
			error.exit();
		}
	}


public:
	explicit ASTKSPSyntaxCheck(NodeProgram *main) : m_def_provider(main->def_provider) {
		m_program = main;
	}

	static NodeAST* fix_memory_exhausted_error(NodeAST& node) {
		MemoryExhaustedNesting memory_exhausted_nesting;
		return node.accept(memory_exhausted_nesting);
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);

		static KSPDeclarations declarations;
		auto new_node = node.accept(declarations);
		static KSPPersistency persistency;
		return new_node->accept(persistency);
	}

	NodeAST* visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten(true);
		return &node;
	};

	NodeAST* visit(NodeArray& node) override {
		node.size->accept(*this);
		check_max_array_size(&node);
		return &node;
	}

	NodeAST* visit(NodeArrayRef& node) override {
		if(node.index) node.index->accept(*this);
		if(node.needs_get_ui_id()) {
			return node.replace_with(std::move(node.wrap_in_get_ui_id()));
		}
		return &node;
	}

	NodeAST* visit(NodeVariableRef& node) override {
		if(node.needs_get_ui_id()) {
			return node.replace_with(std::move(node.wrap_in_get_ui_id()));
		}
		return &node;
	}

	NodeAST* visit(NodeUIControl& node) override {
		m_ui_control_count[node.ui_control_type]++;
		check_max_ui_controls(&node);
		node.control_var->accept(*this);
		node.params->accept(*this);
		return &node;
	}


};