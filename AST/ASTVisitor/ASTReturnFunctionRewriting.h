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
#include "ASTReturnParamPromotion.h"

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
		m_program = &node;


		/// promote return values to parameters of func_defs
		ASTReturnParamPromotion param_promotion(m_def_provider);
		for(auto & func_def : node.function_definitions) {
			func_def->accept(param_promotion);
		}
		// call hoisting after func lowering to profit from correct data types in definition params
		FunctionCallHoisting hoisting;
		node.accept(hoisting);

		/// do immediate inlining of return-only functions
//		ASTExpressionFunctionInlining inlining(m_def_provider);
//		node.accept(inlining);
		node.reset_function_visited_flag();
		m_program->global_declarations->accept(*this);
		for(auto & callback : node.callbacks) {
			callback->accept(*this);
		}
		node.reset_function_visited_flag();

//		/// vector to house only the definitions that are actually used in the program
//		std::vector<std::shared_ptr<NodeFunctionDefinition>> final_function_definitions;
//		// this is needed in case a return only function is called in a function with return value
//		for(auto & func_def : node.function_definitions) {
//			if(m_used_function_definitions.find(func_def.get()) != m_used_function_definitions.end()) {
//				final_function_definitions.push_back(func_def);
//			}
//		}
//		node.function_definitions = final_function_definitions;
		node.update_function_lookup();

		return &node;
	};

	inline NodeAST* visit(NodeFunctionCall& node) override {
		node.function->accept(*this);
		if(node.bind_definition(m_program)) {
			if(!node.get_definition()->visited) node.get_definition()->accept(*this);
			if(node.get_definition()->is_expression_function()) return &node;

			// add throwaway variable ref to params
			if(node.parent->get_node_type() == NodeType::Statement) {
				if(node.is_builtin_kind()) return &node;
				if (node.get_definition() and node.get_definition()->num_return_params > 0) {
					auto &throwaway_var = node.get_definition()->header->get_param(0);
					auto throwaway_ref = throwaway_var->to_reference();
					throwaway_ref->name = m_def_provider->get_fresh_name("_");
					throwaway_ref->kind = NodeReference::Kind::Throwaway;
					node.function->prepend_arg(std::move(throwaway_ref));
				}
			}
		}

		return &node;
	}

	inline NodeAST * visit(NodeFunctionHeaderRef& node) override {
		// make sure that the function that is arg in a higher-order function
		// does not get deleted because it is only ref and not being called
		// foo(bar: (): void) -> bar is not called but function ref
		if(node.get_declaration() and node.is_func_arg()) {
			if(auto def = node.get_declaration()->parent->cast<NodeFunctionDefinition>()) {
				if(!def->visited) def->accept(*this);
			}
		}
		if(node.args) node.args->accept(*this);
		return &node;
	}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		node.visited = true;
//		m_used_function_definitions.insert(&node);
		node.header->accept(*this);
		if(node.return_variable.has_value())
			node.return_variable.value()->accept(*this);
		node.body->accept(*this);
		return &node;
	}

	// if assigned functioncall has return parameter -> make it argument
	// replace single assignment with function call
	// Pre Lowering:
	// var := func_call()
	// Post Lowering:
	// func_call(var)
	inline NodeAST* visit(NodeSingleAssignment &node) override {
		node.r_value->accept(*this);
		node.l_value->accept(*this);
		if(auto func_call = node.r_value->cast<NodeFunctionCall>()) {
			if(!func_call->bind_definition(m_program)) return &node;
			if(func_call->get_definition()->is_expression_function()) return &node;
//			if(func_call->kind != NodeFunctionCall::Kind::UserDefined) return &node;
			if(func_call->is_builtin_kind()) return &node;

			if(func_call->get_definition()->num_return_params > 0) {
				func_call->function->prepend_arg(std::move(node.l_value));
				return node.replace_with(std::move(node.r_value));
			}
		}
		return &node;
	}

	// Pre Lowering:
	// declare var := func_call()
	// Post Lowering:
	// declare var;
	// func_call(var);
	inline NodeAST* visit(NodeSingleDeclaration &node) override {
		if (!node.value) return &node;
		node.value->accept(*this);
		if (auto func_call = node.value->cast<NodeFunctionCall>()) {
			if (!func_call->bind_definition(m_program)) return &node;
			if(func_call->get_definition()->is_expression_function()) return &node;
//			if (func_call->kind != NodeFunctionCall::Kind::UserDefined) return &node;
			if(func_call->is_builtin_kind()) return &node;
			if (func_call->get_definition()->num_return_params > 0) {
				func_call->function->prepend_arg(node.variable->to_reference());
				auto node_block = std::make_unique<NodeBlock>(node.tok);
				node_block->scope = false;
				node_block->add_as_stmt(
					std::make_unique<NodeSingleDeclaration>(std::move(node.variable), nullptr, node.tok));
				node_block->add_as_stmt(std::move(node.value));
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