//
// Created by Mathias Vatter on 31.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"
#include "../../Lowering/ASTLowering.h"
#include "../../Optimization/FunctionCallHoisting.h"
#include "ASTExpressionFunctionInlining.h"
#include "../../Optimization/ParameterMarking.h"

class ASTReturnFunctionRewriting: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
	/// set of all function_definitions that are actually used in program
	/// can only house called functions with no params
	std::set<NodeFunctionDefinition*> m_used_function_definitions;
public:
	inline explicit ASTReturnFunctionRewriting(DefinitionProvider *definition_provider): m_def_provider(definition_provider) {}

	// TODO: Call Function Rewriting after Lowering -> call Function Call Hoisting first and then do return statement rewriting as parameters
	inline NodeAST* visit(NodeProgram& node) override {
		/// do immediate inlining of return-only functions
		ASTExpressionFunctionInlining inlining(m_def_provider);
		node.accept(inlining);

		m_program = &node;

		/// lower func_defs
		for(auto & func_def : node.function_definitions) {
			func_def->lower(m_program);
		}
		// call hoisting after func lowering to profit from correct data types in definition params
		FunctionCallHoisting hoisting;
		node.accept(hoisting);

		m_program->global_declarations->accept(*this);
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();

		/// vector to house only the definitions that are actually used in the program
		std::vector<std::unique_ptr<NodeFunctionDefinition>> final_function_definitions;
		// this is needed in case a return only function is called in a function with return value
		for(auto & func_def : node.function_definitions) {
			if(m_used_function_definitions.find(func_def.get()) != m_used_function_definitions.end()) {
				final_function_definitions.push_back(std::move(func_def));
			}
		}
		node.function_definitions = std::move(final_function_definitions);
		node.update_function_lookup();

		return &node;
	};

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.get_definition(m_program)) {
			if(!node.definition->visited) node.definition->accept(*this);

			// add throwaway variable ref to args
			if(node.parent->get_node_type() == NodeType::Statement) {
				if(node.is_builtin_kind()) return &node;
				if (node.definition and node.definition->num_return_params > 0) {
					auto throwaway_var = static_cast<NodeDataStructure *>(node.definition->header->args->params[0].get());
					auto throwaway_ref = throwaway_var->to_reference();
					throwaway_ref->name = m_def_provider->get_fresh_name("_");
					throwaway_ref->kind = NodeReference::Kind::Throwaway;
					node.function->prepend_arg(std::move(throwaway_ref));
				}
			}
		}

		return &node;
	}

	inline NodeAST * visit(NodeFunctionVarRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.declaration and node.is_func_arg()) {
			if(node.declaration->parent->get_node_type() == NodeType::FunctionDefinition) {
				auto def = static_cast<NodeFunctionDefinition*>(node.declaration->parent);
				if(!def->visited) def->accept(*this);
			}
		}
		node.header->accept(*this);
		return &node;
	}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		node.visited = true;
		m_used_function_definitions.insert(&node);
		node.header->accept(*this);
		if(node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		return &node;
	}

	// if assigned functioncall has return parameter -> make it argument
	// replace single assignment with function call
	inline NodeAST* visit(NodeSingleAssignment &node) override {
		node.r_value->accept(*this);
		node.l_value->accept(*this);
		if(node.r_value->get_node_type() == NodeType::FunctionCall) {
			auto func_call = static_cast<NodeFunctionCall*>(node.r_value.get());
			if(!func_call->get_definition(m_program)) return &node;
//			if(func_call->kind != NodeFunctionCall::Kind::UserDefined) return &node;
			if(func_call->is_builtin_kind()) return &node;

			if(func_call->definition->num_return_params > 0) {
				func_call->function->prepend_arg(std::move(node.l_value));
				return node.replace_with(std::move(node.r_value));
			}
		}
		return &node;
	}

	inline NodeAST* visit(NodeSingleDeclaration &node) override {
		if (!node.value) return &node;
		node.value->accept(*this);
		if (node.value->get_node_type() == NodeType::FunctionCall) {
			auto func_call = static_cast<NodeFunctionCall *>(node.value.get());
			if (!func_call->get_definition(m_program)) return &node;
//			if (func_call->kind != NodeFunctionCall::Kind::UserDefined) return &node;
			if(func_call->is_builtin_kind()) return &node;
			if (func_call->definition->num_return_params > 0) {
				func_call->function->prepend_arg(node.variable->to_reference());
				auto node_block = std::make_unique<NodeBlock>(node.tok);
				node_block->scope = true;
				node_block->add_stmt(std::make_unique<NodeStatement>(
					std::make_unique<NodeSingleDeclaration>(std::move(node.variable), nullptr, node.tok), node.tok));
				node_block->add_stmt(std::make_unique<NodeStatement>(std::move(node.value), node.tok));
				return node.replace_with(std::move(node_block));
			}
		}
		return &node;
	}

	inline NodeAST* visit(NodeBlock& node) override {
		for(auto & stmt : node.statements) {
			stmt->accept(*this);
		}
		node.flatten();
		return &node;
	};
};