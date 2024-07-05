//
// Created by Mathias Vatter on 04.07.24.
//

#pragma once

#include "ASTDesugaring.h"
#include "../misc/Gensym.h"

/// raises functions with deprecated style return to return statement by substituting them
/// when encountering return statement with multiple return values -> parameter promotion
class DesugarFunctionDef : public ASTDesugaring {
private:
	std::stack<NodeFunctionDefinition*> m_functions_visited;
	Gensym m_gensym;
public:
	explicit DesugarFunctionDef(NodeProgram *program) : ASTDesugaring(program) {};

	void inline visit(NodeFunctionDefinition &node) override {
		m_functions_visited.push(&node);
		if(node.return_variable.has_value()) {
			node.num_return_params = 1;
		}
		node.body->accept(*this);
		m_functions_visited.pop();
	};

	void inline visit(NodeSingleAssignment &node) override {
		if(m_functions_visited.top()->return_variable.has_value()) {
			if(node.l_value->get_string() == m_functions_visited.top()->return_variable.value()->name) {
				std::vector<std::unique_ptr<NodeAST>> returns;
				returns.push_back(std::move(node.r_value));
				auto node_return = std::make_unique<NodeReturn>(std::move(returns), node.tok);
				m_functions_visited.top()->return_variable.reset();
				node.replace_with(std::move(node_return));
				return;
			}
		}
		node.l_value->accept(*this);
		node.r_value->accept(*this);
	};

	inline bool throw_shadow_error(NodeReference *ref) {
		if(m_functions_visited.top()->return_variable.has_value()) {
			if(ref->name == m_functions_visited.top()->return_variable.value()->name) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", ref->tok);
				error.m_message = "Variable Reference shadows return variable.";
				error.print();
				return true;
			}
		}
		return false;
	}

	void inline visit(NodeVariableRef &node) override {
		throw_shadow_error(&node);
		m_gensym.ingest(node.name);
	}

	void inline visit(NodeVariable &node) override {
		m_gensym.ingest(node.name);
	}

	void inline visit(NodeArrayRef &node) override {
		throw_shadow_error(&node);
		m_gensym.ingest(node.name);
	}

	void inline visit(NodeArray &node) override {
		m_gensym.ingest(node.name);
	}

	void inline visit(NodeNDArrayRef &node) override {
		throw_shadow_error(&node);
		m_gensym.ingest(node.name);
	}

	void inline visit(NodeNDArray &node) override {
		m_gensym.ingest(node.name);
	}

	void inline visit(NodeReturn &node) override {
		if(m_functions_visited.top()->return_variable.has_value()) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "No Return Statement allowed in <FunctionDefinition> using deprecated return syntax.";
			error.exit();
		}
		if(node.return_variables.size() != m_functions_visited.top()->num_return_params) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "Return Statement has incorrect number of return values.";
			error.m_expected = m_functions_visited.top()->num_return_params;
			error.m_got = node.return_variables.size();
			error.exit();
		}

		// parameter promotion when multiple return values present
		if(node.return_variables.size() > 1) {
			auto block_replace = std::make_unique<NodeBlock>(Token());
			for(int i = 1; i<node.return_variables.size(); i++) {
				auto new_param = std::make_unique<NodeVariable>(
					std::nullopt,
					m_gensym.fresh("ret"),
					TypeRegistry::Unknown,
					DataType::Return, node.return_variables[i]->tok
				);
				auto new_param_ref = new_param->to_reference();
				m_functions_visited.top()->header->args->add_param(std::move(new_param));
				auto node_assignment = std::make_unique<NodeSingleAssignment>(std::move(new_param_ref), std::move(node.return_variables[i]), node.tok);
				block_replace->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), node.tok));
			}
			node.return_variables.erase(std::remove(node.return_variables.begin(), node.return_variables.end(), nullptr), node.return_variables.end());
			block_replace->add_stmt(std::make_unique<NodeStatement>(std::move(node.clone()), node.tok));
			node.replace_with(std::move(block_replace));
		}

	}

};