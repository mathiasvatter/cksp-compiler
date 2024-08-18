//
// Created by Mathias Vatter on 18.08.24.
//

#pragma once

#include "ASTVisitor.h"

/**
 * Adds persistence functions to variables that are declared with a persistence keyword.
 * Is assigned value to a declaration statement is not constant, split the declaration into a declaration and an assignment statement.
 */
class ASTKSPSyntaxCheck: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
	std::vector<NodeDataStructure*> m_persistent_vars;
public:
	explicit ASTKSPSyntaxCheck(DefinitionProvider *definition_provider) : m_def_provider(definition_provider) {};

	NodeAST* visit(NodeSingleDeclaration& node) override {
		node.variable->accept(*this);
		std::unique_ptr<NodeBlock> body = nullptr;
		if(node.variable->persistence.has_value()) {
			body = add_read_functions(node.variable->persistence.value(), node.variable.get());
		}
		if(node.value) {
			node.value->accept(*this);
			if(!node.value->is_constant()) {
				if(!body) {
					body = std::make_unique<NodeBlock>(node.tok);
				}
				auto node_var_ref = node.variable->to_reference();
				node_var_ref->match_data_structure(node.variable.get());
				node_var_ref->ty = node.variable->ty;
				body->add_stmt(std::make_unique<NodeStatement>(std::make_unique<NodeSingleAssignment>(std::move(node_var_ref), std::move(node.value), node.tok), node.tok));
				body->prepend_stmt(std::make_unique<NodeStatement>(std::make_unique<NodeSingleDeclaration>(std::move(node.variable), nullptr, node.tok), node.tok));
				return node.replace_with(std::move(body));
			}
		}

		if(body) {
			body->prepend_stmt(std::make_unique<NodeStatement>(node.clone(), node.tok));
			return node.replace_with(std::move(body));
		}
		return &node;
	}

	NodeAST* visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	};



private:
	static std::unique_ptr<NodeBlock> add_read_functions(const Token& persistence, NodeDataStructure* var) {
		auto node_body = std::make_unique<NodeBlock>(var->tok);

		auto it = PERSISTENCE_TOKENS.find(persistence.type);
		if(it == PERSISTENCE_TOKENS.end()) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", var->tok);
			error.m_message = "Persistence keyword not recognized.";
			error.exit();
		}
		auto var_ref = var->to_reference();
		var_ref->match_data_structure(var);
		var_ref->ty = var->ty;
		for(auto &pers_func : it->second) {
			auto make_persistent = std::make_unique<NodeFunctionCall>(
				false,
				std::make_unique<NodeFunctionHeader>(
					pers_func,
					std::make_unique<NodeParamList>(var->tok, var_ref->clone()),
					var->tok
				),
				var->tok
			);
			make_persistent->kind = NodeFunctionCall::Kind::Builtin;
			make_persistent->ty = TypeRegistry::Void;
			node_body->add_stmt(std::make_unique<NodeStatement>(std::move(make_persistent), var->tok));
		}
		return node_body;
	}

};