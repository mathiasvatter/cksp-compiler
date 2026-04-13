//
// Created by Mathias Vatter on 24.10.24.
//

#pragma once

#include "ASTLowering.h"

/// Lowering of Function Definitions with return statements by two methods:
/// - if the function is inlined somewhere (so not transformed into callable ksp function, this pass will:
///    - deleting all statements after return statements in the same block
///    - wrapping defs with return stmts (that are not at the end of the init block) in while loop
///    - placing exit_flag and continue statement after every return statement
/// - if the function is NEVER inlined (so transformed into callable ksp functions:
///    - no bigger rewrite needed: everything stays as is and later on, the empty return stmts will be replaced with exit
class LoweringFunctionDefReturnStmts final : public ASTLowering {
	NodeFunctionDefinition* m_current_function = nullptr;
	std::string m_exit_flag_name;
	NodeDataStructure* m_exit_flag_var = nullptr;
	int m_num_nested_loops = 0;
	bool needs_exit_replacement = false;

	/// returns true if the function definitions return statements need to be rewritten
	/// this is the case if:
	/// - the function has multiple return statements
	/// - the function has one return stmt but its not the last stmt in the block
	/// - (the function consists NOT of only one if statement with a return stmt in each branch)
	static bool needs_rewrite(const NodeFunctionDefinition& def) {
		if(def.num_return_stmts == 0) return false;
		/// function has one return stmt and its the last one
		if(def.num_return_stmts == 1) {
			if(def.body->get_last_statement()->cast<NodeReturn>()) {
				return false;
			}
		}
		// check if function has only one if statement with a return in each branch
		if (def.body->size() == 1) {
			auto stmt = def.body->get_last_statement().get();
			if (auto node_if = stmt->cast<NodeIf>()) {
				auto if_body = node_if->if_body.get();
				auto else_body = node_if->else_body.get();
				if (!if_body->empty() and if_body->get_last_statement()->cast<NodeReturn>()) {
					if (!else_body->empty() and else_body->get_last_statement()->cast<NodeReturn>()) {
						return false;
					}
					if (else_body->empty()) {
						return false;
					}
				}
			}
		}
		/// function has multiple return stmts
		return true;
	}
public:
	explicit LoweringFunctionDefReturnStmts(NodeProgram *program) : ASTLowering(program) {}

	NodeAST* visit(NodeFunctionDefinition& node) override {

		if(!needs_rewrite(node)) return &node;
		// if the function is not inlined, we do not need to rewrite return statements, we will ad an exit statement afterwards
		if (!node.is_inlined) {
			needs_exit_replacement = true;
			m_current_function = &node;
			node.body->accept(*this);
			m_current_function = nullptr;
			needs_exit_replacement = false;
			return &node;
		}

		m_exit_flag_name = m_program->def_provider->get_fresh_name("RETURN_FLAG");
		auto return_flag_decl = get_return_flag_declaration(node.tok, m_exit_flag_name);
		m_exit_flag_var = return_flag_decl->variable.get();
		auto return_flag_ref = m_exit_flag_var->to_reference();
//		return_flag_ref->match_data_structure(m_exit_flag_var);

		m_num_nested_loops = 0;
		m_current_function = &node;
		node.body->accept(*this);
		m_current_function = nullptr;
		// no need to wrap everything if body is only while loop
		if(node.body->statements.size() == 1 && node.body->statements[0]->statement->get_node_type() == NodeType::While) {
			return &node;
		}
		// wrap body in while loop with exit flag declaration
		auto new_body = std::make_unique<NodeBlock>(
			node.tok,
			std::make_unique<NodeStatement>(std::move(return_flag_decl), node.tok),
			std::make_unique<NodeStatement>(
				std::make_unique<NodeWhile>(
					std::make_unique<NodeBinaryExpr>(
						token::EQUAL,
						std::move(return_flag_ref),
						std::make_unique<NodeInt>(0, node.tok),
						node.tok
					),
					std::move(node.body),
					node.tok
				),
				node.tok
			)
		);
		new_body->scope = true;
		new_body->parent = &node;
		node.body = std::move(new_body);
		return &node;
	}

	NodeAST* visit(NodeWhile& node) override {
		if (!m_current_function->is_inlined) {
			return ASTVisitor::visit(node);
		}

		m_num_nested_loops++;
		node.body->accept(*this);
		node.condition = add_return_condition(std::move(node.condition), m_exit_flag_var);
		// if there are nested loops add exit flag if-condition check
		auto block = std::make_unique<NodeBlock>(
			node.tok,
			std::make_unique<NodeStatement>(
				std::make_unique<NodeWhile>(
					std::move(node.condition),
					std::move(node.body),
					node.tok
				),
				node.tok
			),
			std::make_unique<NodeStatement>(get_return_if_check(m_exit_flag_var, node.tok), node.tok)
		);
		return node.replace_with(std::move(block));
	}

	NodeAST* visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			if (needs_exit_replacement) {
				if (stmt->statement->cast<NodeReturn>()) {
					auto block = std::make_unique<NodeBlock>(stmt->statement->tok);
					block->add_as_stmt(std::move(stmt->statement));
					block->add_as_stmt(DefinitionProvider::exit(block->tok));
					stmt->set_statement(std::move(block));
					continue;
				}
			}

			stmt->accept(*this);
		}
		return &node;
	}

	NodeAST* visit(NodeReturn &node) override {
		auto return_flag_ref = m_exit_flag_var->to_reference();
//		return_flag_ref->match_data_structure(m_exit_flag_var);
		auto return_flag_assignment = std::make_unique<NodeSingleAssignment>(
			std::move(return_flag_ref),
			std::make_unique<NodeInt>(1, node.tok),
			node.tok
		);
		auto new_return = std::make_unique<NodeReturn>(std::move(node.return_variables), node.tok);
		new_return->definition = node.definition;

		return node.replace_with(std::make_unique<NodeBlock>(
			node.tok,
			std::make_unique<NodeStatement>(
				std::move(new_return),
				node.tok
			),
			std::make_unique<NodeStatement>(
				std::move(return_flag_assignment),
				node.tok
			),
			std::make_unique<NodeStatement>(
				DefinitionProvider::continu(node.tok),
				node.tok
			)
		));
	}

private:
	/// if(RETURN_FLAG = 1) continue
	static std::unique_ptr<NodeIf> get_return_if_check(NodeDataStructure* return_flag_var, Token &tok) {
		auto return_flag_ref = return_flag_var->to_reference();
//		return_flag_ref->match_data_structure(return_flag_var);
		auto condition = std::make_unique<NodeBinaryExpr>(
			token::EQUAL,
			std::move(return_flag_ref),
			std::make_unique<NodeInt>(1, tok),
			tok
		);
		auto node_if = std::make_unique<NodeIf>(
			std::move(condition),
			std::make_unique<NodeBlock>(
				tok,
				std::make_unique<NodeStatement>(DefinitionProvider::continu(tok), tok)
			),
			std::make_unique<NodeBlock>(tok, true),
			tok
		);
		node_if->if_body->scope = true;
		return node_if;
	}

	/// adds "and RETURN_FLAG = 0" to condition
	static std::unique_ptr<NodeBinaryExpr> add_return_condition(std::unique_ptr<NodeAST> condition, NodeDataStructure* return_flag_var) {
		auto return_flag_ref = return_flag_var->to_reference();
//		return_flag_ref->match_data_structure(return_flag_var);
		auto token = condition->tok;
		auto parent = condition->parent;
		auto new_condition = std::make_unique<NodeBinaryExpr>(
			token::BOOL_AND,
			std::move(condition),
			std::make_unique<NodeBinaryExpr>(
				token::EQUAL,
				std::move(return_flag_ref),
				std::make_unique<NodeInt>(0, token),
				token
			),
			token
		);
		new_condition->parent = parent;
		return std::move(new_condition);
	}

	static std::unique_ptr<NodeSingleDeclaration> get_return_flag_declaration(Token &tok, std::string &flag_name) {
		// add exit flag to condition
		auto return_flag_var = std::make_shared<NodeVariable>(
			std::nullopt,
			flag_name,
			TypeRegistry::Integer,
			tok,
			DataType::Mutable
		);
		return_flag_var->is_local = true;
		return std::make_unique<NodeSingleDeclaration>(
			std::move(return_flag_var),
			nullptr,
			tok
		);
	}


};