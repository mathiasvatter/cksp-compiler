//
// Created by Mathias Vatter on 21.07.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../misc/Gensym.h"

class FunctionCallHoisting : public ASTVisitor {
private:
//	Gensym m_gensym;
	NodeStatement* m_last_stmt = nullptr;
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_stmt;
	/// function call is hoistable when userdefined and returns values
	/// or is in condition statement
	static inline bool is_hoistable(NodeFunctionCall* node, NodeStatement* last_stmt) {
		if(node->kind == NodeFunctionCall::Kind::Builtin) return false;
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		if(!node->definition) {
			error.m_message = "Function "+node->function->name+" has not been defined and cannot be rewritten.";
			error.m_got = node->function->name;
			error.exit();
		}
		bool is_userdefined = node->kind == NodeFunctionCall::Kind::UserDefined;
//		bool is_param = node->parent->get_node_type() == NodeType::ParamList and node->parent->parent->get_node_type() == NodeType::FunctionHeader;
//		bool is_in_condition = last_stmt->statement->get_node_type() == NodeType::If ||
//			last_stmt->statement->get_node_type() == NodeType::While || last_stmt->statement->get_node_type() == NodeType::Select;
		bool returns_values = node->definition and node->definition->num_return_params > 0;
		bool is_in_stmt = node->parent->get_node_type() == NodeType::Statement;
		// do not hoist if in declaration -> we do not need an extra declaration var
		bool is_in_declaration = node->parent->get_node_type() == NodeType::SingleDeclaration;
		if(!is_in_stmt and !returns_values) {
			error.m_message = "Function "+node->function->name+" does not return any value";
			error.m_got = node->function->name;
			error.exit();
		}
		if(!returns_values) {
			return false;
		}
		return is_userdefined and !is_in_stmt and !is_in_declaration;
	}

	std::unique_ptr<NodeBlock> declare_throwaway_variables() {
		auto node_block = std::make_unique<NodeBlock>(Token());
		auto throwaway_var = std::make_unique<NodeVariable>(
			std::nullopt,
			m_program->def_provider->get_fresh_name("_"),
			nullptr,
			DataType::Mutable,
			Token()
			);
		static std::vector<Type*> types = {TypeRegistry::Integer, TypeRegistry::String, TypeRegistry::Real};
		for(int i = 0; i<3; i++) {
			throwaway_var->ty = types[i];
			node_block->add_stmt(std::make_unique<NodeStatement>(
				std::make_unique<NodeSingleDeclaration>(
					clone_as<NodeDataStructure>(throwaway_var.get()),
					nullptr,
					Token()
					),
					Token()
				));
		}
		return node_block;
	}

public:

	inline NodeAST * visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
//		m_program->global_declarations->prepend_body(declare_throwaway_variables());
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
		return &node;
	};

	inline NodeAST* visit(NodeSingleDeclaration &node) override {
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);
		return &node;
	}

	inline NodeAST * visit(NodeStatement& node) override {
		m_last_stmt = &node;
		node.statement->accept(*this);
		return &node;
	}

	inline NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.get_definition(m_program, true);
		if(is_hoistable(&node, m_last_stmt)) {
			// clone return variable from function definition
			auto return_var = clone_as<NodeDataStructure>(node.definition->header->args->params[0].get());
			// get throaway variable to play return var
//			if(node.parent->get_node_type() == NodeType::Statement) {
//				return_var->name = m_program->def_provider->get_fresh_name("_");
//				return_var->data_type = DataType::Throwaway;
//			} else {
				return_var->name = m_program->def_provider->get_fresh_name("_return");
//			}
			auto return_var_ref = return_var->to_reference();
			return_var_ref->match_data_structure(return_var.get());
			m_declares_per_stmt[m_last_stmt].push_back(
				std::make_unique<NodeSingleDeclaration>(
					std::move(return_var),
					node.clone(),
					node.tok
				)
			);
			return node.replace_with(std::move(return_var_ref));
		}
		return &node;
	}

};