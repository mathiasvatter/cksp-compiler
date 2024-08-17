//
// Created by Mathias Vatter on 04.07.24.
//

#pragma once

#include "ASTLowering.h"
#include "../misc/Gensym.h"

/// Rewrites all return statements
/// promotes parameters of all return statements to return values
class LoweringFunctionDef : public ASTLowering {
private:
	bool throw_deprecated_warning = false;
	std::stack<NodeFunctionDefinition*> m_functions_visited;
	Gensym m_gensym;
public:
	explicit LoweringFunctionDef(NodeProgram *program) : ASTLowering(program) {}

	inline NodeAST* visit(NodeVariableRef &node) override {
		m_gensym.ingest(node.name);
		return &node;
	}
	inline NodeAST* visit(NodeVariable &node) override {
		m_gensym.ingest(node.name);
		return &node;
	}
	inline NodeAST* visit(NodeArrayRef &node) override {
		m_gensym.ingest(node.name);
		return &node;
	}
	inline NodeAST* visit(NodeArray &node) override {
		m_gensym.ingest(node.name);
		return &node;
	}
	inline NodeAST* visit(NodeNDArrayRef &node) override {
		m_gensym.ingest(node.name);
		return &node;
	}
	inline NodeAST* visit(NodeNDArray &node) override {
		m_gensym.ingest(node.name);
		return &node;
	}

	inline NodeAST* visit(NodeFunctionDefinition &node) override {
		m_functions_visited.push(&node);
		m_gensym.refresh();
		if(node.return_variable.has_value()) {
			node.num_return_params = 1;
			if(!throw_deprecated_warning) {
				LoweringFunctionDef::throw_function_deprecation_error(node.return_variable.value()->tok);
				throw_deprecated_warning = true;
			}
			node.return_variable.value()->data_type = DataType::Return;
			node.header->args->prepend_param(std::move(node.return_variable.value()));
			node.return_variable.reset();
		}
		node.body->accept(*this);
		m_functions_visited.pop();
		return &node;
	};

	inline static CompileError throw_function_deprecation_error(Token tok) {
		auto error = CompileError(ErrorType::SyntaxError, "", "<Return> Statement", tok);
		error.m_message = "Deprecated return syntax used in function definition. For type safety, using"
						  " multiple return values or returning arrays, use <return> statement syntax instead.";
		return error;
	}

	inline NodeAST* visit(NodeReturn &node) override {
		if(node.return_variables.size() != m_functions_visited.top()->num_return_params) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "Return Statement has incorrect number of return values.";
			error.m_expected = m_functions_visited.top()->num_return_params;
			error.m_got = node.return_variables.size();
			error.exit();
		}

		// parameter promotion of return parameters
		// replace parameter placeholders instantiated in Desugaring with copies of return values when references
		auto block_replace = std::make_unique<NodeBlock>(Token());
		for(int i = 0; i<node.return_variables.size(); i++) {
			auto& node_return = node.return_variables[i];
			// if return parameter is a reference -> replace with copy
			std::unique_ptr<NodeDataStructure> new_param = nullptr;
			if(auto node_ref = cast_node<NodeReference>(node_return.get())) {
				if(node_ref->ty->get_type_kind() == TypeKind::Basic) {
					new_param = std::make_unique<NodeVariable>(
						std::nullopt,
						m_gensym.fresh("return"),
						node_ref->ty,
						DataType::Param, node.return_variables[i]->tok
					);
				} else if(node_ref->ty->get_type_kind() == TypeKind::Composite) {
					// 	return Note.velocities[self, *]
					// check for wildcards and lower dimension count when encountering wildcard -> array
					if(node_ref->get_node_type() == NodeType::NDArrayRef) {
						auto node_ndarray = static_cast<NodeNDArrayRef*>(node_ref);
						auto dimensions = node_ndarray->indexes->params.size() - node_ndarray->num_wildcards();
						node_ndarray->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node_ref->ty->get_element_type(), dimensions);
					}
					if(node_ref->ty->get_dimensions() == 1) {
						new_param = std::make_unique<NodeArray>(
							std::nullopt,
							m_gensym.fresh("return"),
							node_ref->ty,
							nullptr,
							node_return->tok
						);
					} else {
						if(node_ref->get_node_type() != NodeType::NDArrayRef) {
							auto error = CompileError(ErrorType::InternalError, "", "", node_ref->tok);
							error.m_message = "Got incorrect type of return parameter. Expected NDArrayRef.";
							error.exit();
						}
						auto node_ndarray_ref = static_cast<NodeNDArrayRef*>(node_ref);
						new_param = std::make_unique<NodeNDArray>(
							std::nullopt,
							m_gensym.fresh("return"),
							node_ref->ty,
							std::move(node_ndarray_ref->sizes),
							node_return->tok
						);
					}
				} else {
					auto error = CompileError(ErrorType::InternalError, "", "", node_return->tok);
					error.m_message = "Return Type not supported. Object Pointers should not exist anymore.";
					error.exit();
				}
			// when param list -> array return var
			} else if (node_return->get_node_type() == NodeType::ParamList) {
				new_param = std::make_unique<NodeArray>(
					std::nullopt,
					m_gensym.fresh("return"),
					TypeRegistry::add_composite_type(CompoundKind::Array, node_return->ty->get_element_type()),
					nullptr,
					node_return->tok
					);
			// if not array -> then var
			} else {
				new_param = std::make_unique<NodeVariable>(
					std::nullopt,
					m_gensym.fresh("return"),
					node_return->ty,
					DataType::Param, node.return_variables[i]->tok
					);
			}
			new_param->data_type = DataType::Param;
			auto new_param_ref = new_param->to_reference();
			new_param_ref->ty = node_return->ty;
			new_param_ref->data_type = DataType::Param;
			// replace param in function header in case more than 1 return param
			if(i>0) {
				m_functions_visited.top()->header->args->params[i]->replace_with(std::move(new_param));
			// add them to front in case i = 0
			} else {
				m_functions_visited.top()->header->args->prepend_param(std::move(new_param));
			}
			auto node_assignment = std::make_unique<NodeSingleAssignment>(std::move(new_param_ref), std::move(node.return_variables[i]), node.tok);
			block_replace->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), node.tok));
		}
		return node.replace_with(std::move(block_replace));
	}


};