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

class ASTReturnFunctionRewriting: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
public:
	inline explicit ASTReturnFunctionRewriting(DefinitionProvider *definition_provider): m_def_provider(definition_provider) {}

	// TODO: Call Function Rewriting after Lowering -> call Function Call Hoisting first and then do return statement rewriting as parameters
	inline NodeAST* visit(NodeProgram& node) override {
		/// do immediate inlining of return-only functions
		ASTExpressionFunctionInlining inlining(m_def_provider);
		node.accept(inlining);
		node.debug_print();

		m_program = &node;
		m_program->global_declarations->accept(*this);

		/// lower func_defs
		for(auto & func_def : node.function_definitions) {
			func_def->lower(m_program);
		}
		// call hoisting after func lowering to profit from correct data types in definition params
		FunctionCallHoisting hoisting;
		node.accept(hoisting);

		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.merge_function_definitions();
		return &node;
	};

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.get_definition(m_program)) {
			node.definition->accept(*this);
		}
		return &node;
	}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		node.header->accept(*this);
		if(node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		return &node;
	}

	// if assigned functioncall has return parameter -> make it argument
	// replace single assignment with function call
	inline NodeAST* visit(NodeSingleAssignment &node) override {
		if(node.r_value->get_node_type() == NodeType::FunctionCall) {
			auto func_call = static_cast<NodeFunctionCall*>(node.r_value.get());
			if(!func_call->get_definition(m_program)) return &node;
			if(func_call->definition->num_return_params > 0) {
				func_call->function->args->prepend_param(std::move(node.l_value));
				return node.replace_with(std::move(node.r_value));
			}
		}
		return &node;
	}

	inline NodeAST* visit(NodeSingleDeclaration &node) override {
		if(!node.value) return &node;
		if(node.value->get_node_type() == NodeType::FunctionCall) {
			auto func_call = static_cast<NodeFunctionCall*>(node.value.get());
			if(!func_call->get_definition(m_program)) return &node;
			if(func_call->definition->num_return_params > 0) {
				func_call->function->args->prepend_param(node.variable->to_reference());
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

	// if statement is func call and function is return function but does not return anything:
	// transform into assign statement with throwaway var "_"
	inline NodeAST* visit(NodeStatement &node) override {
		node.statement->accept(*this);
		if(node.statement->get_node_type() == NodeType::FunctionCall) {
			auto func_call = static_cast<NodeFunctionCall*>(node.statement.get());
			if(func_call->definition and func_call->definition->num_return_params>0) {
				auto throwaway_var = static_cast<NodeDataStructure*>(func_call->definition->header->args->params[0].get());
				auto throwaway_ref =throwaway_var->to_reference();
				throwaway_ref->name = m_def_provider->get_fresh_name("_");
				throwaway_ref->kind = NodeReference::Kind::Throwaway;
				auto assignment = std::make_unique<NodeSingleAssignment>(
					std::move(throwaway_ref),
					std::move(node.statement),
					node.tok
				);
				node.statement = std::move(assignment);
				node.statement->parent = &node;
				node.statement->accept(*this);
			}
		}
		return &node;
	}

};