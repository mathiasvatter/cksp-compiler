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
class VariablePruning : public ASTOptimizations {
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
			if(!reference->get_declaration()->is_used) {
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
		if(!is_prune_candidate(node.variable.get())) {
			return &node;
		}
		// claim everything as unused at first
		node.variable->is_used = false;
		m_all_declarations.push_back(&node);
		return &node;
	}

	/// decide whether to potentially prune or not
	/// ui controls will not be pruned
	/// persistent data structures will not be pruned
	static inline bool is_prune_candidate(NodeDataStructure* node) {
		if(node->get_node_type() == NodeType::UIControl) {
			return false;
		}
		if(node->data_type == DataType::Const) return false;

		if(node->persistence.has_value()) {
			return false;
		}
		return true;
	}

	// is unused if not ui_control and only used as l_value in assignments (if not arrayref) -> adds these assignments to vector
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

	inline NodeAST *visit(NodeSingleAssignment& node) override {
		if(node.l_value->get_node_type() == NodeType::VariableRef) {
			auto var_ref = static_cast<NodeVariableRef*>(node.l_value.get());
			if(var_ref->kind == NodeReference::Kind::Throwaway) {
				return node.replace_with(std::make_unique<NodeDeadCode>(node.tok));
			}
		}

		node.l_value->accept(*this);
		node.r_value->accept(*this);
		return &node;
	}

	inline NodeAST *visit(NodeVariableRef &node) override {
		if(node.kind == NodeReference::Kind::Throwaway) {
			// if a throwaway variable is not in assignment it has been incorrectly used
			if(node.parent->get_node_type() != NodeType::SingleAssignment) {
				auto error = get_raw_compile_error(ErrorType::VariableError, node);
				error.m_message  = "Throwaway variables <"+node.name+"> are removed by the compiler and will not be included "
																	 "in the compiled code. Consider renaming your variable.";
				error.exit();
			}
		}
		node.get_declaration()->is_used |= is_used(node);
		return &node;
	}


	inline NodeAST *visit(NodeArrayRef &node) override {
		if(node.index) node.index->accept(*this);
		node.get_declaration()->is_used |= is_used(node);
		return &node;
	}

};

