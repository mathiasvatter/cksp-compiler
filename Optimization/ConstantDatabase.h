//
// Created by Mathias Vatter on 03.07.26.
//

#pragma once
#include "ConstantPropagation.h"
#include "../AST/ASTVisitor/ASTOptimizations.h"

/**
 * visits the values and tries to simplify them by using the database
 */
class ConstantDatabaseConstantFolding final : public ASTOptimizations {
	std::unordered_map<NodeDataStructure*, std::unique_ptr<NodeStatement>>& m_constant_vars;

public:
	explicit ConstantDatabaseConstantFolding(std::unordered_map<NodeDataStructure*, std::unique_ptr<NodeStatement>>& map)
		: m_constant_vars(map) {}

	NodeAST* run(NodeAST& node) {
		auto new_node = node.do_constant_folding();
		auto newer_node = new_node->accept(*this);
		return newer_node->do_constant_folding();
	}

private:
	NodeAST* visit(NodeVariableRef & node) override {
		if (auto decl = node.get_declaration()) {
			if (decl->data_type == DataType::Const) {
				auto it = m_constant_vars.find(decl.get());
				if (it != m_constant_vars.end()) {
					return node.replace_with(it->second->statement->clone());
				}
			}
		}
		return &node;
	}
};

/**
 * This class holds a database with constant variables and their values
 */
class ConstantDatabase final : public ASTOptimizations {
	/// constants as keys -> value
	// std::unordered_map<StringTypeKey, std::unique_ptr<NodeAST>, StringTypeKeyHash> m_db;
	std::unordered_map<NodeDataStructure*, std::unique_ptr<NodeStatement>> m_constant_vars;

	// std::unique_ptr<NodeBlock> m_constants;
public:
	NodeAST* build(NodeProgram &node) {
		m_program = &node;
		m_constant_vars.clear();

		m_program->global_declarations->accept(*this);
		m_program->init_callback->accept(*this);
		parallel_for_each(node.callbacks.begin(), node.callbacks.end(),
			[this](const auto & callback) {
				  if (callback.get() == m_program->init_callback) return;
				callback->accept(*this);
			}
		);
		parallel_for_each(node.function_definitions.begin(), node.function_definitions.end(),
			[this](const auto & func) {
				func->accept(*this);
			}
		);

		node.reset_function_visited_flag();


		for (auto& con : m_constant_vars) {
			std::cout << con.first->name << " := " << con.second->statement->get_string() << std::endl;
		}
		return &node;
	}



private:

	void add_constant(NodeDataStructure* data, std::unique_ptr<NodeAST> value) {
		if(data->data_type == DataType::Const) {
			auto tok = value->tok;
			auto stmt = std::make_unique<NodeStatement>(std::move(value), tok);
			static ConstantDatabaseConstantFolding cf(m_constant_vars);
			cf.run(*stmt);
			m_constant_vars[data] = std::move(stmt);
		}
	}

	std::unique_ptr<NodeAST> get_constant(const NodeReference& node) {
		auto decl = node.get_declaration();
		if (decl and decl->data_type == DataType::Const) {
			auto it = m_constant_vars.find(decl.get());
			if (it != m_constant_vars.end()) {
				return it->second->clone();
			}
		}
		return nullptr;
	}

	NodeAST* visit(NodeSingleDeclaration& node) override {
		// node.variable->accept(*this);
		if (node.value) {
			node.value->accept(*this);
			if (!node.variable->cast<NodeVariable>()) return &node;
			if (node.variable->data_type == DataType::Const) {
				add_constant(node.variable.get(), node.value->clone());
			}
		}
		return &node;
	}

};

