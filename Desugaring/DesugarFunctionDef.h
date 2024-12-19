//
// Created by Mathias Vatter on 04.07.24.
//

#pragma once

#include "ASTDesugaring.h"
#include "../misc/Gensym.h"

/// raises functions with deprecated style return to return statement only if expression only function
/// because other functions with old-style return may be expected to work in a certain way
/// when encountering return statement with multiple return values -> dummy parameter promotion
/// when encountering old return style -> throw deprecation warning
class DesugarFunctionDef : public ASTDesugaring {
private:
	bool throw_deprecated_warning = false;
	NodeFunctionDefinition* m_current_function = nullptr;
	int m_num_last_ret_values = 0; // saves the number of return values of last return statement

	inline static CompileError throw_function_deprecation_error(Token tok) {
		auto error = CompileError(ErrorType::SyntaxError, "", "<Return> Statement", tok);
		error.m_message = "Deprecated return syntax used in function definition. For type safety, using"
						  " multiple return values or returning arrays, use <return> statement syntax instead.";
		return error;
	}

	/// transforms expression only function into return statement functions
	static inline bool transform_expr_only_to_return_function(NodeFunctionDefinition* def) {
		if(!def->return_variable.has_value()) return false;
		if(def->body->statements.size() == 1) {
			auto stmt = def->body->get_last_statement().get();
			if(auto assignment = stmt->cast<NodeSingleAssignment>()) {
				if(assignment->l_value->get_string() == def->return_variable.value()->name) {
					auto node_return = std::make_unique<NodeReturn>(assignment->tok, std::move(assignment->r_value));
					stmt->replace_with(std::move(node_return));
					def->return_variable.reset();
					return true;
				}
			}
		}
		return false;
	}


public:
	explicit DesugarFunctionDef(NodeProgram *program) : ASTDesugaring(program) {};

	inline NodeAST* visit(NodeFunctionDefinition &node) override {
		m_current_function = &node;
		if(node.return_variable.has_value()) {
			node.num_return_params = 1;
			if(!throw_deprecated_warning) {
				throw_function_deprecation_error(node.return_variable.value()->tok).print();
				throw_deprecated_warning = true;
			}
			transform_expr_only_to_return_function(&node);
		}
		m_num_last_ret_values = node.num_return_params;
		node.body->accept(*this);
		m_current_function = nullptr;
		return &node;
	};

	inline NodeAST* visit(NodeSingleAssignment &node) override {
		node.l_value->accept(*this);
		node.r_value->accept(*this);
		return &node;
	};

	// add dummy variable for every return parameter >1 to function header
	inline NodeAST* visit(NodeReturn &node) override {
		if (m_current_function->return_variable.has_value()) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "No Return Statement allowed in <FunctionDefinition> using deprecated return syntax.";
			error.exit();
		}
		if(node.return_variables.size() != m_num_last_ret_values) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "<Return Statement> has incorrect number of return values. When using multiple <Return Statements>, all"
							  " must have the same number of return values.";
			error.m_expected = std::to_string(m_num_last_ret_values);
			error.m_got = std::to_string(node.return_variables.size());
			error.exit();
		}
		m_current_function->num_return_stmts++;
		m_current_function->return_stmts.push_back(&node);
		// parameter promotion when multiple return values present
		if(node.return_variables.size() > 1 and m_current_function->num_return_stmts == 1) {
			for(int i = 1; i<node.return_variables.size(); i++) {
				auto new_param = std::make_unique<NodeVariable>(
					std::nullopt,
					"ret$"+std::to_string(i),
					TypeRegistry::Unknown,
					DataType::Return, node.return_variables[i]->tok
				);
				m_current_function->header->add_param(std::move(new_param));
			}
		}
		return &node;
	};

};