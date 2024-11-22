//
// Created by Mathias Vatter on 21.07.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../misc/Gensym.h"

/// Hoist function calls that return values and are userdefined out of control flow into a separate declaration
class FunctionCallHoisting : public ASTVisitor {
private:
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_stmt;
	/// function call is hoistable when userdefined and returns values and is not an expression function
	/// or is in condition statement
	static inline bool is_hoistable(NodeFunctionCall* node) {
		// not hoistable functions are: Undefined, Builtin, Property
		if(node->is_builtin_kind()) return false;
		auto error = CompileError(ErrorType::SyntaxError, "", "", node->tok);
		if(!node->get_definition()) {
			return false;
		}
		bool returns_values = node->get_definition() and node->get_definition()->num_return_params > 0;
		bool is_in_stmt = node->parent->cast<NodeStatement>();
		// do not hoist if in declaration -> we do not need an extra declaration var
		bool is_in_declaration = node->parent->cast<NodeSingleDeclaration>();
		bool is_in_assignment =  node->parent->cast<NodeSingleAssignment>();
		bool is_in_return = node->parent->cast<NodeReturn>();
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

	/// insert declarations from m_declares_per_stmt into the dedicated statements
	inline void insert_calls_in_statements() {
		for(auto & stmt : m_declares_per_stmt) {
			auto node_body = std::make_unique<NodeBlock>(stmt.first->tok);
			node_body->scope = true;
			for(auto &decl : stmt.second) {
				node_body->add_as_stmt(std::move(decl));
			}
			node_body->add_as_stmt(std::move(stmt.first->statement));
			stmt.first->set_statement(std::move(node_body));
		}
	}

	inline NodeAST * visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(auto & def : node.function_definitions) {
			def->accept(*this);
		}

		insert_calls_in_statements();
		return &node;
	};

	inline NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		if(node.get_definition() and node.get_definition()->is_expression_function()) return &node;

		if(is_hoistable(&node)) {
			auto last_stmt = node.get_parent_statement();
			if(!last_stmt) {
				auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
				error.m_message = "Unable to find parent statement of <FunctionCall>.";
				error.exit();
			}
			// clone return variable from function definition
			auto return_var = clone_shared(node.get_definition()->header->get_param(0));
			return_var->name = m_program->def_provider->get_fresh_name("_ret");
			auto ref = return_var->to_reference();
			ref->match_data_structure(return_var);
			m_declares_per_stmt[last_stmt].push_back(
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

};