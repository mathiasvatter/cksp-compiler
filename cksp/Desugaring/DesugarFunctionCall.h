//
// Created by Mathias Vatter on 04.10.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Desugaring of Function calls with multiple arguments that are not in assignment/declaration context
 * -> only use first return and fill up with throwaway params
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
					node.function->set_args(inline_message_parameters(node.function->args));
				}
			}
		}

		if (node.function->get_num_args() > 0 and node.function->name == "num_elements") {
			node.kind = NodeFunctionCall::Kind::Builtin;
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			if (node.function->get_num_args() > 2) {
				error.m_message = "Too many arguments for function call <num_elements>.";
				error.exit();
			} else if (node.function->get_num_args() < 1) {
				error.m_message = "Too few arguments for function call <num_elements>.";
				error.exit();
			}
			std::unique_ptr<NodeReference> array = nullptr;
			if(is_instance_of<NodeReference>(node.function->get_arg(0).get())) {
				array = unique_ptr_cast<NodeReference>(std::move(node.function->get_arg(0)));
			} else {
				error.m_message = "First argument for function call <num_elements> must be a reference.";
				error.exit();
			}
			std::unique_ptr<NodeAST> dimension = nullptr;
			if (node.function->get_num_args() == 2) {
				dimension = std::move(node.function->get_arg(1));
			}
			auto num_elements = std::make_unique<NodeNumElements>(std::move(array), std::move(dimension), node.tok);
			return node.replace_with(std::move(num_elements));
		}

		if((node.function->get_num_args() == 2 || node.function->get_num_args() == 4) &&
			(node.function->name == "search" || node.function->name == "sort")) {
			node.kind = NodeFunctionCall::Kind::Builtin;
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			if(is_instance_of<NodeReference>(node.function->get_arg(0).get())) {
				auto search = std::make_unique<NodeSortSearch>(
					node.function->name,
					unique_ptr_cast<NodeReference>(std::move(node.function->get_arg(0))),
					std::move(node.function->get_arg(1)),
					node.tok
				);
				if(search->name == "search") {
					search->ty = TypeRegistry::Integer;
				} else {
					search->ty = TypeRegistry::Void;
				}
				if(node.function->get_num_args() == 4) {
					search->set_from(std::move(node.function->get_arg(2)));
					search->set_to(std::move(node.function->get_arg(3)));
				}
				return node.replace_with(std::move(search));
			} else {
				error.m_message = "First argument for function call <search> must be a reference.";
				error.exit();
			}
		}

		if(node.function->get_num_args() == 1 and node.function->name == "use_count") {
			node.kind = NodeFunctionCall::Kind::Builtin;
			auto use_count = std::make_unique<NodeUseCount>(unique_ptr_cast<NodeReference>(std::move(node.function->get_arg(0))));
			return node.replace_with(std::move(use_count));
		}

		if(node.function->get_num_args() == 1 and node.function->name == "pairs") {
			node.kind = NodeFunctionCall::Kind::Builtin;
			auto pairs = std::make_unique<NodePairs>(std::move(node.function->get_arg(0)));
			return node.replace_with(std::move(pairs));
		}

		if(node.function->get_num_args() >= 1 and node.function->get_num_args() <= 3 and node.function->name == "range") {
			node.kind = NodeFunctionCall::Kind::Builtin;
			std::unique_ptr<NodeAST> start = std::make_unique<NodeInt>(0, node.tok);
			std::unique_ptr<NodeAST> stop;
			std::unique_ptr<NodeAST> step = std::make_unique<NodeInt>(1, node.tok);
			if(node.function->get_num_args() == 1) {
				stop = std::move(node.function->get_arg(0));
			} else {
				start = std::move(node.function->get_arg(0));
				stop = std::move(node.function->get_arg(1));
				if(node.function->get_num_args() == 3) {
					step = std::move(node.function->get_arg(2));
				}
			}
			auto range = std::make_unique<NodeRange>(
				std::move(start),
				std::move(stop),
				std::move(step),
				node.tok
			);
			return node.replace_with(std::move(range));
		}

		node.bind_definition(m_program);
		if(!node.get_definition()) return &node;
		if(node.get_definition()->num_return_params <= 1) return &node;
		if(node.parent->get_node_type() == NodeType::SingleDeclaration or node.parent->get_node_type() == NodeType::SingleAssignment) {
			return &node;
		}

		auto num_throwaway_args = node.get_definition()->num_return_params - 1;
		for(int i = 0; i < num_throwaway_args; i++) {
			auto throwaway_ref = std::make_unique<NodeVariableRef>("_", node.tok);
			throwaway_ref->kind = NodeVariableRef::Kind::Throwaway;
			node.function->prepend_arg(std::move(throwaway_ref));
		}

		return &node;
	}

private:

	CompileError throw_insufficient_args_error(Token tok) {
		auto error = CompileError(ErrorType::SyntaxError, "", "", tok);
		error.m_message = "Too few arguments for function call <"+tok.val+">.";
		return error;
	}

	static std::unique_ptr<NodeParamList> inline_message_parameters(std::unique_ptr<NodeParamList>& params) {
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
		// Füge das endgültige node_expr der neuen Parameterliste hinzu
		new_param->add_param(std::move(node_expr));
		return new_param;
	}
};