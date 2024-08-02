//
// Created by Mathias Vatter on 31.07.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../ASTNodes/AST.h"
#include "../../BuiltinsProcessing/DefinitionProvider.h"
#include "../../Lowering/ASTLowering.h"
#include "../../Optimization/FunctionCallHoisting.h"

class ASTReturnFunctionRewriting: public ASTVisitor {
private:
	DefinitionProvider *m_def_provider;
	NodeDataStructure* m_throwaway_var = nullptr;
public:
	inline explicit ASTReturnFunctionRewriting(DefinitionProvider *definition_provider): m_def_provider(definition_provider) {}

	// TODO: Call Function Rewriting after Lowering -> call Function Call Hoisting first and then do return statement rewriting as parameters
	inline NodeAST* visit(NodeProgram& node) override {

		m_program = &node;
		m_program->global_declarations->accept(*this);
//		for(auto & struct_def : node.struct_definitions) {
//			struct_def->accept(*this);
//		}
		for(auto & function_definition : node.function_definitions) {
			function_definition->accept(*this);
		}
		// call hoisting afterwards to profit from correct data types in definition params
		FunctionCallHoisting hoisting;
		node.accept(hoisting);

		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.merge_function_definitions();
		return &node;
	};

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		node.lower(m_program);
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

};