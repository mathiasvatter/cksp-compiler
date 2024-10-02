//
// Created by Mathias Vatter on 09.08.24.
//

#pragma once


#include "ASTFunctionInlining.h"
#include "../../Lowering/LoweringFunctionDef.h"

/// Immediate Inlining of return-only functions
/// Substitution already takes place in the parent class ASTFunctionInlining in the following nodes:
/// NodeArrayRef
/// NodeNDArrayRef
/// NodeVariableRef
/// NodeFunctionHeader
class ASTExpressionFunctionInlining: public ASTFunctionInlining {
private:
	bool deprecated_warning = false;
public:
	inline explicit ASTExpressionFunctionInlining(DefinitionProvider *definition_provider) : ASTFunctionInlining(definition_provider) {}

	NodeAST* visit(NodeProgram &node) override {
		node.reset_function_used_flag();
		m_program = &node;
		m_program->global_declarations->accept(*this);
		for(auto & struct_def : node.struct_definitions) {
			struct_def->accept(*this);
		}
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();
		/// vector to house only the definitions that are actually used in the program
		std::vector<std::unique_ptr<NodeFunctionDefinition>> final_function_definitions;
		for(auto & func_def : node.function_definitions) {
			if(func_def->is_used) {
				final_function_definitions.push_back(std::move(func_def));
			}
		}
		node.function_definitions = std::move(final_function_definitions);
		node.reset_function_used_flag();
		node.update_function_lookup();
		return &node;
	}

	/// only transforms expression only function into return statement functions
	inline bool transform_to_return_function(NodeFunctionDefinition* def) {
		if(!def->return_variable.has_value()) return false;
		if(!deprecated_warning) {
			LoweringFunctionDef::throw_function_deprecation_error(def->return_variable.value()->tok).print();
			deprecated_warning = true;
		}
		if(def->body->statements.size() == 1) {
			auto stmt = def->body->statements[0]->statement.get();
			if(stmt->get_node_type() == NodeType::SingleAssignment) {
				auto assignment = static_cast<NodeSingleAssignment*>(stmt);
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

	NodeAST* visit(NodeFunctionCall &node) override {
		node.function->accept(*this);

		if(node.get_definition(m_program)) {
			if(node.kind != NodeFunctionCall::Kind::UserDefined) return &node;
			if(!node.definition->visited) {
				node.definition->accept(*this);
			}
			node.definition->visited = true;
			// if it is not an expression-only function, do not transform into return statement, instead add return variable to function header
			transform_to_return_function(node.definition);
			// see if the function is a return-only function
			if(is_expression_function(node.definition)) {

				m_program->function_call_stack.push(node.definition);

				auto node_func_body = clone_as<NodeBlock>(node.definition->body.get());
				m_substitution_stack.push(get_substitution_map(node.definition->header.get(), node.function->header.get()));
				node_func_body->accept(*this);

				m_substitution_stack.pop();
				m_program->function_call_stack.pop();
//				return node.replace_with(get_expression_func_return(node_func_body.get()));
				return node.replace_with(get_expression_return(node_func_body.get()));
			} else {
				node.definition->is_used = true;
			}
		}
		return &node;
	}

	NodeAST * visit(NodeFunctionVarRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.declaration and node.is_func_arg()) {
			if(node.declaration->parent->get_node_type() == NodeType::FunctionDefinition) {
				auto def = static_cast<NodeFunctionDefinition*>(node.declaration->parent);
				def->is_used = true;
			}
		}
		node.header->accept(*this);
		return &node;
	}

	static inline bool is_expression_function(NodeFunctionDefinition* def) {
		if(def->num_return_params > 0) {
			if(def->return_variable.has_value()) return false;
			if(def->body->statements.size() == 1) {
				auto stmt = def->body->statements[0]->statement.get();
				if(stmt->get_node_type() == NodeType::Return) {
					return true;
				}
			}
		}
		return false;
	}

	static inline std::unique_ptr<NodeAST> get_expression_func_return(NodeFunctionDefinition* def) {
		auto stmt = def->body->statements[0]->statement.get();
		if(stmt->get_node_type() == NodeType::Return) {
			auto ret = static_cast<NodeReturn*>(stmt);
			return std::move(ret->return_variables[0]);
		}
		auto error = CompileError(ErrorType::InternalError, "", "", def->tok);
		error.m_message = "Function is not a return-only function";
		error.exit();
		return nullptr;
	}

	static inline std::unique_ptr<NodeAST> get_expression_return(NodeBlock* body) {
		auto stmt = body->statements[0]->statement.get();
		if(stmt->get_node_type() == NodeType::Return) {
			auto ret = static_cast<NodeReturn*>(stmt);
			return std::move(ret->return_variables[0]);
		}
		auto error = CompileError(ErrorType::InternalError, "", "", body->tok);
		error.m_message = "Function is not a return-only function";
		error.exit();
		return nullptr;
	}


};