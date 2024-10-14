//
// Created by Mathias Vatter on 21.07.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../misc/Gensym.h"

/// Hoist function calls that return values and are userdefined out of control flow into a separate declaration
/// Hoist array or ndarray initializer lists out of function args into separate declarations
class FunctionCallHoisting : public ASTVisitor {
private:
//	Gensym m_gensym;
	NodeStatement* m_last_stmt = nullptr;
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_stmt;
	/// function call is hoistable when userdefined and returns values
	/// or is in condition statement
	static inline bool is_hoistable(NodeFunctionCall* node) {
//		if(node->kind != NodeFunctionCall::Kind::UserDefined) return false;
		// not hoistable functions are: Undefined, Builtin, Property
		if(node->is_builtin_kind()) return false;
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		if(!node->definition) {
//			error.m_message = "Function "+node->function->name+" has not been defined and cannot be rewritten.";
//			error.m_got = node->function->name;
//			error.exit();
			return false;
		}
//		bool is_userdefined = node->kind == NodeFunctionCall::Kind::UserDefined;
//		bool is_param = node->parent->get_node_type() == NodeType::ParamList and node->parent->parent->get_node_type() == NodeType::FunctionHeader;
//		bool is_in_condition = last_stmt->statement->get_node_type() == NodeType::If ||
//			last_stmt->statement->get_node_type() == NodeType::While || last_stmt->statement->get_node_type() == NodeType::Select;
		bool returns_values = node->definition and node->definition->num_return_params > 0;
		bool is_in_stmt = node->parent->get_node_type() == NodeType::Statement;
		// do not hoist if in declaration -> we do not need an extra declaration var
		bool is_in_declaration = node->parent->get_node_type() == NodeType::SingleDeclaration;
		bool is_in_assignment =  node->parent->get_node_type() == NodeType::SingleAssignment;
		if(!is_in_stmt and !returns_values) {
			error.m_message = "Function "+node->function->name+" does not return any value";
			error.m_got = node->function->name;
			error.exit();
		}
		if(!returns_values) {
			return false;
		}
		return true and !is_in_stmt and !is_in_declaration and !is_in_assignment;
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
		for(auto & def : node.function_definitions) {
			def->accept(*this);
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
		node.get_definition(m_program);
		if(is_hoistable(&node)) {
			std::unique_ptr<NodeReference> ref = nullptr;
			// clone return variable from function definition
			auto return_var = clone_as<NodeDataStructure>(node.definition->header->args->params[0].get());
			return_var->name = m_program->def_provider->get_fresh_name("_return");
			ref = return_var->to_reference();
			ref->match_data_structure(return_var.get());
			m_declares_per_stmt[m_last_stmt].push_back(
				std::make_unique<NodeSingleDeclaration>(
					std::move(return_var),
					node.clone(),
					node.tok
				)
			);
			return node.replace_with(std::move(ref));
		}
		return &node;
	}

	inline NodeAST * visit(NodeParamList& node) override {
		for(auto & param : node.params) {
			param->accept(*this);
		}
		// if node is not an array initializer list, return node
		if(node.parent->get_node_type() == NodeType::ParamList and node.parent->parent->get_node_type() == NodeType::FunctionHeader) {
			auto func_call = static_cast<NodeFunctionCall*>(node.parent->parent->parent->parent);
			if(!func_call->definition) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Function "+func_call->function->name+" has not been defined and cannot be rewritten with an array initializer.";
				error.m_got = func_call->function->name;
				error.exit();
			}


			node.flatten();

			// if initializer list inside function header -> hoist out
			auto array_node = std::make_unique<NodeNDArray>(
				std::nullopt,
				m_program->def_provider->get_fresh_name("_init_array"),
				TypeRegistry::Unknown,
				nullptr, //std::make_unique<NodeInt>(node.params.size(), node.tok),
				node.tok
			);
			auto node_declaration = std::make_unique<NodeSingleDeclaration>(
				std::move(array_node),
				node.clone(),
				node.tok
			);
			auto array_ref = node_declaration->variable->to_reference();
			array_ref->match_data_structure(node_declaration->variable.get());
			m_declares_per_stmt[m_last_stmt].push_back(std::move(node_declaration));
			return node.replace_with(std::move(array_ref));
		}
		return &node;
	}

};