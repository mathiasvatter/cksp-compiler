//
// Created by Mathias Vatter on 21.07.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

class FunctionCallHoisting : public ASTVisitor {
private:
	NodeStatement* m_last_stmt = nullptr;
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_stmt;
	/// function call is hoistable when userdefined and is nested in other function call
	/// or in condition statement
	static inline bool is_hoistable(NodeFunctionCall* node, NodeStatement* last_stmt) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		if(!node->definition) {
			error.m_message = "Function "+node->function->name+" has not been defined";
			error.m_got = node->function->name;
			error.exit();
		}
		if(node->definition and node->definition->num_return_params == 0) {
			error.m_message = "Function "+node->function->name+" does not return any value";
			error.m_got = node->function->name;
			error.exit();
		}
		bool is_userdefined = node->kind == NodeFunctionCall::Kind::UserDefined;
		bool is_in_func_call = last_stmt->statement->get_node_type() == NodeType::FunctionCall;
		bool is_in_condition = last_stmt->statement->get_node_type() == NodeType::If ||
			last_stmt->statement->get_node_type() == NodeType::While || last_stmt->statement->get_node_type() == NodeType::Select;
		return is_userdefined and (is_in_func_call or is_in_condition);
	}

public:

	inline void visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
		}

		for(auto & stmt : m_declares_per_stmt) {
			auto node_body = std::make_unique<NodeBlock>(node.tok);
			node_body->scope = true;
			for(auto &decl : stmt.second) {
				node_body->add_stmt(std::make_unique<NodeStatement>(std::move(decl), node.tok));
			}
			node_body->add_stmt(std::make_unique<NodeStatement>(std::move(stmt.first->statement), stmt.first->tok));
			stmt.first->statement = std::move(node_body);
			stmt.first->statement->parent = stmt.first;
		}
	};

	inline void visit(NodeStatement& node) override {
		m_last_stmt = &node;
		node.statement->accept(*this);
	}

	inline void visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.get_definition(m_program, true);
		if(is_hoistable(&node, m_last_stmt)) {
			auto return_var = std::make_unique<NodeVariable>(
				std::nullopt,
				node.function->name,
				node.ty,
				DataType::Mutable,
				node.tok
			);
			auto return_var_ref = return_var->to_reference();
			return_var_ref->match_data_structure(return_var.get());
			m_declares_per_stmt[m_last_stmt].push_back(
				std::make_unique<NodeSingleDeclaration>(
					std::move(return_var),
					node.clone(),
					node.tok
				)
			);
			node.replace_with(std::move(return_var_ref));
		}
	}

};