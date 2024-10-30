//
// Created by Mathias Vatter on 17.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * Marks function parameter references for easier substitution
 */
class ParameterMarking : public ASTVisitor {
private:
	std::stack<std::unordered_set<std::string>> m_function_params;
	inline bool mark_function_param_ref(NodeReference* ref) {
		if(m_program->function_call_stack.empty()) return false;
		if(m_function_params.empty()) return false;
		if(ref->data_type == DataType::Param) return true;

		const auto & set = m_function_params.top();
		auto it = set.find(ref->name);
		if(it != set.end()) {
			ref->data_type = DataType::Param;
			return true;
		}
		return false;
	}
public:

	/// check for used functions
	inline NodeAST *visit(NodeProgram &node) override {
		m_program = &node;
		node.update_function_lookup();
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		return &node;
	}

	inline NodeAST *visit(NodeFunctionCall &node) override {
		// visit header
		node.function->accept(*this);
		if(node.kind != NodeFunctionCall::Kind::UserDefined) return &node;

		if(node.get_definition(m_program)) {
			m_program->function_call_stack.push(node.definition);
			if (!node.definition->visited) {
				std::unordered_set<std::string> function_params;
				for (auto &param : node.definition->header->params->params) {
					function_params.insert(static_cast<NodeDataStructure *>(param.get())->name);
				}
				m_function_params.push(function_params);
				node.definition->body->accept(*this);
				m_function_params.pop();
				node.definition->visited = true;
			}
			m_program->function_call_stack.pop();
		}
		return &node;
	}

	/// do marking as DataType::Param
	inline NodeAST *visit(NodeNDArrayRef &node) override {
		if(node.indexes) node.indexes->accept(*this);
		mark_function_param_ref(&node);
		return &node;
	}
	/// do marking as DataType::Param
	inline NodeAST *visit(NodeArrayRef &node) override {

		if(node.index) node.index->accept(*this);
		mark_function_param_ref(&node);
		return &node;
	}
	/// do marking as DataType::Param
	inline NodeAST *visit(NodeVariableRef &node) override {
		mark_function_param_ref(&node);
		return &node;
	}
};