//
// Created by Mathias Vatter on 21.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTOptimizations.h"

/// needs working and fresh m_references_per_data_structure from Definition Provider
/// promoted constants here do not have to have constant assignments
/// promoted constants have only at most one assignment throughout the whole program
class ConstantPromotion : public ASTOptimizations {
private:
	/// saves constant candidates and their values
	std::unordered_map<std::shared_ptr<NodeDataStructure>, std::unique_ptr<NodeAST>> m_constant_candidates;
	std::vector<NodeReference*> m_constant_candidate_references;
public:

	NodeAST* visit(NodeProgram& node) override {
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
		promote_constants();
		return &node;
	};

	inline bool promote_constants() {
		for (auto & var : m_constant_candidates) {
			if(var.second) {
				auto declaration = static_cast<NodeSingleDeclaration*>(var.first->parent);
				declaration->variable->data_type = DataType::Const;
				declaration->variable->persistence = std::nullopt;
				declaration->value = std::move(var.second);
				declaration->value->parent = declaration;
			}
		}
		for(auto & ref : m_constant_candidate_references) {
			if(ref->declaration->data_type == DataType::Const) {
				ref->data_type = DataType::Const;
			}
		}
		return true;
	}

	/// add specific candidates to the map
	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);
		if(is_constant_candidate(*node.variable)) {
			// add to constant candidates -> if it has a value, add it, otherwise add nullptr
			add_to_constant_candidates(node);
		}
		return &node;
	}

	/// remove var from constant candidates if reassigned or used value-altering builtin function call
	NodeAST * visit(NodeVariableRef& node) override {
		if(node.data_type == DataType::Const) return &node;
		if(is_in_constant_candidates_map(node.declaration)) {
			// if it gets reassigned, check if it already has a value, if yes, remove from constant candidates
			if(node.is_l_value()) {
				auto assignment = static_cast<NodeSingleAssignment*>(node.parent);
				// adds value to constant candidates if it has none yet.
				if(!assignment->r_value->is_constant()) {
					// if the value is not constant, remove from constant candidates
					remove_from_constant_candidates(&node);
				}
				if(!add_value_to_constant_candidates(&node, assignment->r_value)) {
					// had already a value assigned to it -> remove from constant candidates
					remove_from_constant_candidates(&node);
				}
			} else if(is_value_altering_func_arg(&node)) {
				remove_from_constant_candidates(&node);
			}
			m_constant_candidate_references.push_back(&node);
		}
		return &node;
	}

	/// add to map including its value if it has one, otherwise add nullptr
	bool add_to_constant_candidates(const NodeSingleDeclaration& node) {
		if(node.value and !node.value->is_constant()) return false;
		m_constant_candidates[node.variable] = node.value ? node.value->clone() : nullptr;
		return true;
	}

	bool remove_from_constant_candidates(NodeVariableRef* node) {
		m_constant_candidates.erase(node->declaration);
		return true;
	}

	/// if it already has a value, return false, if not, add value to constant candidates and return true
	bool add_value_to_constant_candidates(NodeVariableRef* ref, std::unique_ptr<NodeAST>& value) {
		auto it = m_constant_candidates.find(ref->declaration);
		if(it != m_constant_candidates.end()) {
			if(it->second) return false;
			it->second = value->clone();
			return true;
		}
		return false;
	}

	bool is_in_constant_candidates_map(std::shared_ptr<NodeDataStructure> node) {
		return m_constant_candidates.find(node) != m_constant_candidates.end();
	}

	static bool is_constant_candidate(const NodeDataStructure& node) {
		if(!node.is_local and node.data_type == DataType::Mutable and node.get_node_type() == NodeType::Variable) {
			return node.ty != TypeRegistry::String;
		}
		return false;
	}


};