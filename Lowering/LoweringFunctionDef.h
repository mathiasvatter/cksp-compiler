//
// Created by Mathias Vatter on 24.10.24.
//

#pragma once

#include "ASTLowering.h"

/// Lowering of Function Definitions with return statements by
/// - deleting all statements after return statements in the same block
/// - wrapping defs with return stmts (that are not at the end of the init block) in while loop
/// - placing exit_flag and continue statement after every return statement
/// -
class LoweringFunctionDef : public ASTLowering {
private:
	NodeFunctionDefinition* m_current_function = nullptr;
	std::string m_exit_flag_name;
	NodeDataStructure* m_exit_flag_var = nullptr;
	int m_num_nested_loops = 0;

	/// returns true if the function definitions return statements need to be rewritten
	/// this is the case if:
	/// - the function has multiple return statements
	/// - the function has one return stmt but its not the last stmt in the block
	/// - (the function consists NOT of only one if statement with a return stmt in each branch)
	static bool needs_rewrite(NodeFunctionDefinition& def) {
		if(def.num_return_stmts == 0) return false;
		/// function has one return stmt and its the last one
		if(def.num_return_stmts == 1) {
			if(def.body->statements.back()->statement->get_node_type() == NodeType::Return) {
				return false;
			}
		}
		/// function has multiple return stmts
		return true;
	}
public:
	explicit LoweringFunctionDef(NodeProgram *program) : ASTLowering(program) {}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		if(!needs_rewrite(node)) return &node;
		m_exit_flag_name = m_program->def_provider->get_fresh_name("RETURN_FLAG");
		auto return_flag_decl = get_exit_flag_declaration(node.tok, m_exit_flag_name);
		m_exit_flag_var = return_flag_decl->variable.get();
		auto return_flag_ref = m_exit_flag_var->to_reference();
		return_flag_ref->match_data_structure(m_exit_flag_var);

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

	inline NodeAST* visit(NodeWhile& node) override {
		m_num_nested_loops++;
		node.body->accept(*this);
		node.condition = add_return_condition(std::move(node.condition), m_exit_flag_var);
		if(m_num_nested_loops > 1) {
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
		return &node;
	}

	inline NodeAST* visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		return &node;
	}

	inline NodeAST* visit(NodeReturn &node) override {
		auto return_flag_ref = m_exit_flag_var->to_reference();
		return_flag_ref->match_data_structure(m_exit_flag_var);
		auto return_flag_assignment = std::make_unique<NodeSingleAssignment>(
			std::move(return_flag_ref),
			std::make_unique<NodeInt>(1, node.tok),
			node.tok
		);
		auto new_return = std::make_unique<NodeStatement>(
			std::make_unique<NodeReturn>(std::move(node.return_variables), node.tok),
			node.tok
			);
		return node.replace_with(std::make_unique<NodeBlock>(
			node.tok,
			std::move(new_return),
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
		return_flag_ref->match_data_structure(return_flag_var);
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


//	/// continue
//	static std::unique_ptr<NodeFunctionCall> get_continue_call(Token &tok) {
//		auto call = std::make_unique<NodeFunctionCall>(
//			false,
//			std::make_unique<NodeFunctionVarRef>(
//				std::make_unique<NodeFunctionHeader>(
//					"continue",
//					std::make_unique<NodeParamList>(
//						tok
//					),
//					tok
//				),
//				tok
//			),
//			tok
//		);
//		call->function->header->has_forced_parenth = false;
//		return call;
//	}

	/// adds "and RETURN_FLAG = 0" to condition
	static std::unique_ptr<NodeBinaryExpr> add_return_condition(std::unique_ptr<NodeAST> condition, NodeDataStructure* return_flag_var) {
		auto return_flag_ref = return_flag_var->to_reference();
		return_flag_ref->match_data_structure(return_flag_var);
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

	static std::unique_ptr<NodeSingleDeclaration> get_exit_flag_declaration(Token &tok, std::string &flag_name) {
		// add exit flag to condition
		auto exit_flag_var = std::make_unique<NodeVariable>(
			std::nullopt,
			flag_name,
			TypeRegistry::Integer,
			DataType::Mutable,
			tok
		);
		exit_flag_var->is_local = true;
		return std::make_unique<NodeSingleDeclaration>(
			std::move(exit_flag_var),
			nullptr,
			tok
		);
	}


};