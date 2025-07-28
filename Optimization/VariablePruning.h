//
// Created by Mathias Vatter on 17.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"

/**
 * Removes variables that are unused
 * Determines if variable is unused if it is
 * - not a ui_control
 * - only used as l_value in assignments
 * - never used as r_value
 * Also removes throwaway variables
 */
class VariablePruning final : public ASTOptimizations {

	std::vector<NodeSingleDeclaration*> m_all_declarations;
	std::vector<NodeSingleAssignment*> m_all_assignments;
public:

	NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(const auto & func_def : node.function_definitions) {
			func_def->accept(*this);
		}
		node.reset_function_visited_flag();
		prune_unused_variables();
		return &node;
	};

	/// deletes unused variables by removing declarations and assignments
	void prune_unused_variables() const {
		for(auto &ass: m_all_assignments) {
			if(!ass->l_value->get_declaration()->is_used) {
				ass->remove_node();
			}
		}
		for(auto &decl : m_all_declarations) {
			if(!decl->variable->is_used) {
				decl->remove_node();
			}
		}
	}

	NodeAST *visit(NodeSingleDeclaration &node) override {
		node.variable->accept(*this);
		// claim everything as unused at first
		node.variable->is_used = false;
		if (node.value) {
			// if value is function call, it is used
			node.value->accept(*this);
			node.variable->is_used = node.value->cast<NodeFunctionCall>();
		}
		// get ui control variables out of the picture
		if(!is_prune_candidate(node.variable.get())) {
			return &node;
		}
		m_all_declarations.push_back(&node);
		return &node;
	}

	/// decide whether to potentially prune or not
	/// ui controls will not be pruned
	/// persistent data structures will not be pruned
	static bool is_prune_candidate(const NodeDataStructure* node) {
		if(node->get_node_type() == NodeType::UIControl) {
			return false;
		}
//		if(node->data_type == DataType::Const) return false;

		if(node->persistence.has_value()) {
			return false;
		}
		return true;
	}

	// is unused if not ui_control and only used as l_value in assignments (if not arrayref) -> adds these assignments to vector
	// if it is an l_value and the r_value is a function call -> it shall be used (builtins)
	bool is_used(const NodeReference &node) {
		if(node.data_type != DataType::UIControl) {
			if(const auto assignment = node.is_l_value()) {
				if (assignment->r_value->cast<NodeFunctionCall>()) {
					return true;
				}
				m_all_assignments.push_back(assignment);
				return false;
			}
		}
		return true;
	}

	NodeAST *visit(NodeSingleAssignment& node) override {
		if(auto var_ref = node.l_value->cast<NodeVariableRef>()) {
			if(var_ref->kind == NodeReference::Kind::Throwaway) {
				return node.remove_node();
			}
		}

		node.l_value->accept(*this);
		node.r_value->accept(*this);
		return &node;
	}

	NodeAST *visit(NodeVariableRef &node) override {
		if(node.kind == NodeReference::Kind::Throwaway) {
			// if a throwaway variable is not in assignment it has been incorrectly used
			if(node.parent->cast<NodeSingleAssignment>()) {
				auto error = get_raw_compile_error(ErrorType::VariableError, node);
				error.m_message  = "Throwaway variables <"+node.name+"> are removed by the compiler and will not be included "
																	 "in the compiled code. Consider renaming your variable.";
				error.exit();
			}
		}
		if (auto declaration = node.get_declaration()) {
			declaration->is_used |= is_used(node);
		} else {
			DefinitionProvider::throw_declaration_error(node).exit();
		}
		return &node;
	}


	NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		if (auto declaration = node.get_declaration()) {
			declaration->is_used |= is_used(node);
		} else {
			DefinitionProvider::throw_declaration_error(node).exit();
		}
		return &node;
	}

};

