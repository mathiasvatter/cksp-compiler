//
// Created by Mathias Vatter on 17.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * Removes variables that are unused
 * Determines if variable is unused if it is
 * - not a ui_control
 * - only used as l_value in assignments
 * - never used as r_value
 */
class VariablePruning : public ASTVisitor {
private:
	std::vector<NodeSingleDeclaration*> m_all_declarations;
	std::vector<NodeSingleAssignment*> m_all_assignments;
public:

	inline NodeAST* visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & func_def : node.function_definitions) {
			func_def->accept(*this);
		}
		node.reset_function_visited_flag();
		prune_unused_variables();
		return &node;
	};

	/// deletes unused variables by removing declarations and assignments
	void prune_unused_variables() {
		for(auto &ass: m_all_assignments) {
			auto reference = static_cast<NodeReference*>(ass->l_value.get());
			if(!reference->declaration->is_used) {
				ass->replace_with(std::make_unique<NodeDeadCode>(ass->tok));
			}
		}
		for(auto &decl : m_all_declarations) {
			if(!decl->variable->is_used) {
				decl->replace_with(std::make_unique<NodeDeadCode>(decl->tok));
			}
		}
	}

	inline NodeAST *visit(NodeSingleDeclaration &node) override {
		node.variable->accept(*this);
		if (node.value) node.value->accept(*this);
		// get ui control variables out of the picture
		if(node.variable->get_node_type() == NodeType::UIControl) return &node;
		// claim everything as unused at first
		node.variable->is_used = false;
		m_all_declarations.push_back(&node);
		return &node;
	}

	// is unused if not ui_control and only used as l_value in assignments -> adds these assignments to vector
	inline bool is_used(const NodeReference &node) {
		if(node.data_type != DataType::UIControl) {
			if(node.parent->get_node_type() == NodeType::SingleAssignment) {
				auto assignment = static_cast<NodeSingleAssignment*>(node.parent);
				if(assignment->l_value.get() == &node) {
					m_all_assignments.push_back(assignment);
					return false;
				}
			}
		}
		return true;
	}

	inline NodeAST *visit(NodeVariableRef &node) override {
		node.declaration->is_used |= is_used(node);
		return &node;
	}

	inline NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		node.declaration->is_used |= is_used(node);
		return &node;
	}

};

