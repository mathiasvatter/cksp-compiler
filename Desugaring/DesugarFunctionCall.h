//
// Created by Mathias Vatter on 04.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugaring of Function calls with multiple arguments that are not in assignment/declaration context
 * -> only use first return and fill up with throwaway args
 */
class DesugarFunctionCall : public ASTDesugaring {
public:
	explicit DesugarFunctionCall(NodeProgram *program) : ASTDesugaring(program) {};

	inline NodeAST * visit(NodeFunctionCall& node) override {
		// message overloaded is not recognized as builtin
		// constructor method renaming
		if(node.kind == NodeFunctionCall::Kind::Undefined) {
			if(node.function->get_num_args() > 1) {
				if (node.function->name == "message") {
					// lowering of message parameters when separated by comma
					node.function->header->args = inline_message_parameters(node.function->header->args);
					node.function->header->args->parent = node.function->header->args.get();
				}
//				if (node.function->name == "num_elements") {
//					node.kind = NodeFunctionCall::Kind::Builtin;
//					std::unique_ptr<NodeReference> array = nullptr;
//					if(is_instance_of<NodeReference>(array.get())) {
//						array = std::unique_ptr<NodeReference>(static_cast<NodeReference*>(node.function->header->args->params[0].release()));
//					} else {
//						auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
//						error.m_message = "First argument for function call <num_elements> must be a reference.";
//						error.exit();
//					}
//					if (node.function->header->args->params.size() == 2) {
//						auto dimension = std::move(node.function->header->args->params[1]);
//						auto num_elements =
//							std::make_unique<NodeNumElements>(std::move(array), std::move(dimension), node.tok);
//						num_elements->parent = &node;
//						return node.replace_with(std::move(num_elements));
//					}
//					if (node.function->header->args->params.size() > 2) {
//						auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
//						error.m_message = "Too many arguments for function call <num_elements>.";
//						error.exit();
//					}
//				}
			}
		}

		node.get_definition(m_program);
		if(!node.definition) return &node;
		if(node.definition->num_return_params <= 1) return &node;
		if(node.parent->get_node_type() == NodeType::SingleDeclaration or node.parent->get_node_type() == NodeType::SingleAssignment) {
			return &node;
		}

		auto num_throwaway_args = node.definition->num_return_params - 1;
		for(int i = 0; i < num_throwaway_args; i++) {
			auto throwaway_ref = std::make_unique<NodeVariableRef>("_", node.tok);
			throwaway_ref->kind = NodeVariableRef::Kind::Throwaway;
			node.function->prepend_arg(std::move(throwaway_ref));
		}

		return &node;
	}

private:

	static inline std::unique_ptr<NodeParamList> inline_message_parameters(std::unique_ptr<NodeParamList>& params) {
		// it is already only one parameter
		if (params->params.size() == 1) return std::move(params);

		auto new_param = std::make_unique<NodeParamList>(params->tok);

		// initialize node_expr with last param
		auto node_expr = std::move(params->params.back());
		params->params.pop_back();

		// Durchlaufe die restlichen Parameter in umgekehrter Reihenfolge
		for (int i = params->params.size() - 1; i >= 0; --i) {
			// Erstelle einen neuen NodeString für das Komma
			auto comma_node = std::make_unique<NodeString>("\", \"", params->tok);

			// Erstelle einen neuen NodeBinaryExpr mit dem aktuellen Parameter und dem bisherigen Ausdruck
			auto combined_expr = std::make_unique<NodeBinaryExpr>(
				token::STRING_OP,
				std::move(params->params[i]),
				std::move(comma_node),
				params->tok
			);
			combined_expr->ty = TypeRegistry::String;

			// Hänge den bisherigen Ausdruck an den neuen NodeBinaryExpr an
			node_expr = std::make_unique<NodeBinaryExpr>(
				token::STRING_OP,
				std::move(combined_expr),
				std::move(node_expr),
				params->tok
			);
			node_expr->ty = TypeRegistry::String;
		}
		node_expr->parent = new_param.get();
		// Füge das endgültige node_expr der neuen Parameterliste hinzu
		new_param->add_param(std::move(node_expr));
		return new_param;
	}
};