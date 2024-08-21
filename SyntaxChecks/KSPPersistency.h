//
// Created by Mathias Vatter on 19.08.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * Adds persistence functions to variables that are declared with a persistence keyword.
 */
class KSPPersistency: public ASTVisitor {
public:

	NodeAST* visit(NodeSingleDeclaration& node) override {
		if(node.variable->persistence.has_value()) {
			auto new_declaration = clone_as<NodeSingleDeclaration>(&node);
			auto body = add_persistency_functions(new_declaration->variable.get());
			body->prepend_stmt(std::make_unique<NodeStatement>(std::move(new_declaration), node.tok));
			return node.replace_with(std::move(body));
		}
		if(node.variable->get_node_type() == NodeType::UIControl) {
			auto ui_control = static_cast<NodeUIControl*>(node.variable.get());
			if(ui_control->persistence.has_value()) {
				auto new_declaration = clone_as<NodeSingleDeclaration>(&node);
				auto body = add_persistency_functions(ui_control->control_var.get());
				body->prepend_stmt(std::make_unique<NodeStatement>(std::move(new_declaration), node.tok));
				return node.replace_with(std::move(body));
			}
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
	static std::unique_ptr<NodeBlock> add_persistency_functions(NodeDataStructure* var) {
		auto node_body = std::make_unique<NodeBlock>(var->tok);

		if(!var->persistence.has_value()) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::InternalError, *var);
			error.m_message = "Variable ist not persistent.";
			error.exit();
		}
		auto persistence = var->persistence.value();
		var->persistence = std::nullopt;
		auto it = PERSISTENCE_TOKENS.find(persistence.type);
		if(it == PERSISTENCE_TOKENS.end()) {
			auto error = ASTVisitor::get_raw_compile_error(ErrorType::SyntaxError, *var);
			error.m_message = "Persistence keyword not recognized.";
			error.exit();
		}
		for(auto &pers_func : it->second) {
			auto var_ref = var->to_reference();
			var_ref->match_data_structure(var);
			var_ref->ty = var->ty;
			auto make_persistent = std::make_unique<NodeFunctionCall>(
				false,
				std::make_unique<NodeFunctionHeader>(
					pers_func,
					std::make_unique<NodeParamList>(var->tok, std::move(var_ref)),
					var->tok
				),
				var->tok
			);
			make_persistent->kind = NodeFunctionCall::Kind::Builtin;
			node_body->add_stmt(std::make_unique<NodeStatement>(std::move(make_persistent), var->tok));
		}
		return node_body;
	}

};