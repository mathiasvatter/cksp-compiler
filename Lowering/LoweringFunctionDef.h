//
// Created by Mathias Vatter on 04.07.24.
//

#pragma once

#include "ASTLowering.h"
#include "../misc/Gensym.h"

/// Lowers to retun statements when necessary
class LoweringFunctionDef : public ASTLowering {
private:
	std::stack<NodeFunctionDefinition*> m_functions_visited;

public:
	explicit LoweringFunctionDef(NodeProgram *program) : ASTLowering(program) {}

	void inline visit(NodeFunctionDefinition &node) override {
		m_functions_visited.push(&node);
		if(node.return_variable.has_value()) {
			node.num_return_params = 1;
		}
		node.body->accept(*this);
		m_functions_visited.pop();
	};

	void inline visit(NodeReturn &node) override {
		if(node.return_variables.size() != m_functions_visited.top()->num_return_params) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "Return Statement has incorrect number of return values.";
			error.m_expected = m_functions_visited.top()->num_return_params;
			error.m_got = node.return_variables.size();
			error.exit();
		}

		// parameter promotion when multiple return values present
		// replace parameter placeholders instantiated in Desugaring with copies of return values when references
		if(node.return_variables.size() > 1) {
			auto block_replace = std::make_unique<NodeBlock>(Token());
			for(int i = 1; i<node.return_variables.size(); i++) {
				int ret_param_idx = m_functions_visited.top()->header->args->params.size()-(node.return_variables.size()-1)+(i-1);
				auto name = static_cast<NodeDataStructure*>(m_functions_visited.top()->header->args->params[ret_param_idx].get())->name;
				auto& node_return = node.return_variables[i];
				// if return parameter is a reference -> replace with copy
				std::unique_ptr<NodeDataStructure> new_param = nullptr;
				if(auto node_ref = cast_node<NodeReference>(node_return.get())) {
					if(node_ref->ty->get_type_kind() == TypeKind::Basic) {
						new_param = std::make_unique<NodeVariable>(
							std::nullopt,
							name,
							node_ref->ty,
							DataType::Return, node.return_variables[i]->tok
						);
					} else if(node_ref->ty->get_type_kind() == TypeKind::Composite) {
						new_param = std::make_unique<NodeArray>(
							std::nullopt,
							name,
							node_ref->ty,
							nullptr,
							node_return->tok
						);
					} else {
						auto error = CompileError(ErrorType::InternalError, "", "", node_return->tok);
						error.m_message = "Return Type not supported. Object Pointers should not exist anymore.";
						error.exit();
					}
				// when param list -> array return var
				} else if (node_return->get_node_type() == NodeType::ParamList) {
					new_param = std::make_unique<NodeArray>(
						std::nullopt,
						name,
						TypeRegistry::add_composite_type(CompoundKind::Array, node_return->ty->get_element_type()),
						nullptr,
						node_return->tok
						);
				} else {
					auto error = CompileError(ErrorType::InternalError, "", "", node_return->tok);
					error.m_message = "Return Type not supported.";
					error.exit();
				}
				auto new_param_ref = new_param->to_reference();
				new_param_ref->ty = node_return->ty;

				m_functions_visited.top()->header->args->params[ret_param_idx]->replace_with(std::move(new_param));
				auto node_assignment = std::make_unique<NodeSingleAssignment>(std::move(new_param_ref), std::move(node.return_variables[i]), node.tok);
				block_replace->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), node.tok));
			}
			node.return_variables.erase(std::remove(node.return_variables.begin(), node.return_variables.end(), nullptr), node.return_variables.end());
			block_replace->add_stmt(std::make_unique<NodeStatement>(std::move(node.clone()), node.tok));
			node.replace_with(std::move(block_replace));
		}

	}

};