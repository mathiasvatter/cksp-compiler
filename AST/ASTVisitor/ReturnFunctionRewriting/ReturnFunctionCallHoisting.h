//
// Created by Mathias Vatter on 21.07.24.
//

#pragma once

#include "../ASTVisitor.h"
#include "../../../misc/Gensym.h"

/// Hoist function calls that return values and are userdefined out of control flow into a separate declaration
/**
 * PreLowering:
 * 	if some_func() = 1
 * 		...
 * 	end if
 * PostLowering:
 * 	declare ret0 := some_func()
 * 	if ret0 = 1
 * 		...
 * 	end if
 */
class ReturnFunctionCallHoisting final : public ASTVisitor {
	std::unordered_map<NodeStatement*, std::vector<std::unique_ptr<NodeSingleDeclaration>>> m_declares_per_stmt;
	/// function call is hoistable when userdefined and returns values and is not an expression function
	/// or is in condition statement
	static bool is_hoistable(const NodeFunctionCall& node) {
		// not hoistable functions are: Undefined, Builtin, Property
		if(node.is_builtin_kind()) return false;
		if(!node.get_definition()) {
			return false;
		}
		bool returns_values = node.get_definition() and node.get_definition()->num_return_params > 0;
		bool is_in_stmt = node.parent->cast<NodeStatement>();
		// do not hoist if in declaration -> we do not need an extra declaration var
		bool is_in_declaration = node.parent->cast<NodeSingleDeclaration>();
		bool is_in_assignment =  node.parent->cast<NodeSingleAssignment>();
		bool is_in_return = node.parent->cast<NodeReturn>();
		if(!is_in_stmt and !returns_values) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "Function "+node.function->name+" does not return any value";
			error.m_got = node.function->name;
			error.exit();
		}
		if(!returns_values) {
			return false;
		}
		return true and !is_in_stmt and !is_in_declaration and !is_in_assignment;
	}

public:

	NodeAST* do_function_call_hoisting(NodeFunctionCall& node, NodeProgram* program) {
		m_program = program;
		m_declares_per_stmt.clear();
		auto new_node = node.accept(*this);
		insert_calls_in_statements();
		m_declares_per_stmt.clear();
		return new_node;
	}

	/// insert declarations from m_declares_per_stmt into the dedicated statements
	void insert_calls_in_statements() {
		for(auto &[fst, snd] : m_declares_per_stmt) {
			auto node_body = std::make_unique<NodeBlock>(fst->tok);
			node_body->scope = true;
			for(auto &decl : snd) {
				decl->kind = NodeSingleDeclaration::Kind::ReturnVar;
				node_body->add_as_stmt(std::move(decl));
			}
			node_body->add_as_stmt(std::move(fst->statement));
			fst->set_statement(std::move(node_body));
		}
	}

	NodeAST * visit(NodeProgram& node) override {
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(const auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(const auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		for(const auto & def : node.function_definitions) {
			def->accept(*this);
		}

		insert_calls_in_statements();
		return &node;
	};

	NodeAST * visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		node.bind_definition(m_program);
		if(node.get_definition() and node.get_definition()->is_expression_function()) return &node;

		if(is_hoistable(node)) {
			const auto last_stmt = node.get_parent_statement();
			if(!last_stmt) {
				auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
				error.m_message = "Unable to find parent statement of <FunctionCall>.";
				error.exit();
			}

			// clone return variable from function definition if function was already return param promoted
			auto return_var = clone_shared(node.get_definition()->header->get_param(0));
			return_var->is_local = true;
			return_var->data_type = DataType::Return;
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

			// check for while condition.
			// in case of while conditions, a function call assignments needs to be prepended to the while loop body
			if (const auto node_while = last_stmt->statement->cast<NodeWhile>()) {
				auto assignment = std::make_unique<NodeSingleAssignment>(
					clone_as<NodeReference>(ref.get()),
					node.clone(),
					node.tok
				);
				node_while->body->add_as_stmt(std::move(assignment));
			}

			return node.replace_with(std::move(ref));
		}
		return &node;
	}

};