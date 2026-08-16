//
// Created by Mathias Vatter on 04.07.24.
//

#pragma once

#include "../ASTVisitor.h"

/// Rewrites all return statements
///  - promotes all return values to function parameters utilizing the typesystem
///  - The return parameter names are generated be global Gensym instance
class ReturnParamPromotion final : public ASTVisitor {
	DefinitionProvider* m_def_provider = nullptr;
	std::string m_return_param_name = "ret";
	std::vector<std::string> m_return_param_names;
	NodeFunctionDefinition* m_current_function = nullptr;
public:

	explicit ReturnParamPromotion(NodeProgram* main) : m_def_provider(main->def_provider) {
        m_program = main;
    }

	void set_program(NodeProgram* program) override {
		ASTVisitor::set_program(program);
		m_def_provider = program ? program->def_provider : nullptr;
	}

	void do_return_param_promotion(NodeFunctionDefinition& def) {
		def.accept(*this);
		m_return_param_names.clear();
	}

private:
	NodeAST* visit(NodeFunctionDefinition &node) override {
		m_current_function = &node;
		// do not rewrite if expression function
		if(node.is_expression_function()) return &node;

		if(node.return_variable.has_value()) {
			node.num_return_params = 1;
			node.return_variable.value()->data_type = DataType::Return;

			auto decl = std::make_unique<NodeFunctionParam>(node.return_variable.value(), nullptr, node.tok);
			decl->parent = node.header.get();
			node.header->params.insert(node.header->params.begin(), std::move(decl));
			node.return_variable.reset();
			return &node;
		}

		// generate return parameters
		for (int i = 0; i< node.num_return_params; i++) {
			m_return_param_names.push_back(m_def_provider->get_fresh_name(m_return_param_name));
		}

		m_current_function->return_stmts.clear();
		node.body->accept(*this);
		m_current_function = nullptr;
		return &node;
	};

	NodeAST* visit(NodeReturn &node) override {
		if(node.return_variables.size() != m_current_function->num_return_params) {
			auto error = Diagnostic(ErrorType::SyntaxError, "", "", node.tok);
			error.message = "Return Statement has incorrect number of return values.";
			error.expected = std::to_string(m_current_function->num_return_params);
			error.actual = std::to_string(node.return_variables.size());
			error.exit();
		}
		m_current_function->return_stmts.push_back(&node);
		// parameter promotion of return parameters
		// replace parameter placeholders instantiated in Desugaring with copies of return values when references
		auto block_replace = std::make_unique<NodeBlock>(Token());
		for(int i = 0; i<node.return_variables.size(); i++) {
			auto& node_return = node.return_variables[i];
			std::string return_name = m_return_param_names[i];
			// if return parameter is a reference -> replace with copy
			std::unique_ptr<NodeDataStructure> new_param = nullptr;
			if(auto node_ref = cast_node<NodeReference>(node_return.get())) {
				if(node_ref->ty->get_type_kind() == TypeKind::Basic) {
					new_param = std::make_unique<NodeVariable>(
						std::nullopt,
						return_name,
						node_ref->ty,
						node.return_variables[i]->tok,
						DataType::Return
					);
				} else if(node_ref->ty->get_type_kind() == TypeKind::Composite) {
					// 	return Note.velocities[self, *]
					// check for wildcards and lower dimension count when encountering wildcard -> array
					if(auto node_ndarray = node_ref->cast<NodeNDArrayRef>()) {
						auto dimensions = node_ndarray->indexes->params.size() - node_ndarray->num_wildcards();
						node_ndarray->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node_ref->ty->get_element_type(), dimensions);
					}
					if(node_ref->ty->get_dimensions() == 1) {
						// The return parameter receives the whole array, so it has to be as large as
						// the one that is returned. Callers clone it for the variable that catches the
						// result, and a clone without a size is no valid declaration.
						std::unique_ptr<NodeAST> size = nullptr;
						if(const auto declaration = node_ref->get_declaration()) {
							if(const auto declared_array = declaration->cast<NodeArray>(); declared_array and declared_array->size) {
								size = declared_array->size->clone();
							}
						}
						new_param = std::make_unique<NodeArray>(
							std::nullopt,
							return_name,
							node_ref->ty,
							std::move(size),
							node_return->tok
						);
					} else {
						if(node_ref->get_node_type() != NodeType::NDArrayRef) {
							auto error = Diagnostic(ErrorType::InternalError, "", "", node_ref->tok);
							error.message = "Got incorrect type of return parameter. Expected NDArrayRef.";
							error.exit();
						}
						auto node_ndarray_ref = node_ref->cast<NodeNDArrayRef>();
						new_param = std::make_unique<NodeNDArray>(
							std::nullopt,
							return_name,
							node_ref->ty,
							std::move(node_ndarray_ref->sizes),
							node_return->tok
						);
					}
				} else {
					auto error = Diagnostic(ErrorType::InternalError, "", "", node_return->tok);
					error.message = "Return Type not supported. Object Pointers should not exist anymore.";
					error.exit();
				}
			// when param list -> array return var
			} else if (node_return->get_node_type() == NodeType::InitializerList) {
				new_param = std::make_unique<NodeArray>(
					std::nullopt,
					return_name,
					TypeRegistry::add_composite_type(CompoundKind::Array, node_return->ty->get_element_type()),
					nullptr,
					node_return->tok
					);
			// if not array -> then var
			} else {
				new_param = std::make_unique<NodeVariable>(
					std::nullopt,
					return_name,
					node_return->ty,
					node.return_variables[i]->tok,
					DataType::Return
				);
			}
			new_param->data_type = DataType::Return;
			const bool returns_composite = new_param->ty and new_param->ty->cast<CompositeType>();
			auto new_param_ref = new_param->to_reference();
			new_param_ref->ty = node_return->ty;
			new_param_ref->data_type = DataType::Return;
			// replace param in function header in case more than 1 return param
			// when multiple return statements -> add only return params for the first one
			if(&node == m_current_function->return_stmts[0]) {
				if (i > 0) {
					m_current_function->header->get_param(i)->replace_with(std::move(new_param));
					// add them to front in case i = 0
				} else {
					m_current_function->header->prepend_param(std::move(new_param));
				}
			}
			m_current_function->header->params[0]->kind = NodeInstruction::ReturnVar;
			// An array return parameter is passed by reference like every other array parameter, so
			// the caller's variable is substituted for it when the function is inlined and the
			// returned array is written into it directly. Without this the parameter is promoted
			// to a global array that every call copies out of - one copy and one array too many,
			// and a shared one at that, which a returning function that waits cannot use safely.
			// ASTFunctionStrategy already chose inlining for it, see returns_composite() there.
			if (returns_composite) {
				m_current_function->header->params[i]->is_pass_by_ref = true;
			}

			auto node_assignment = std::make_unique<NodeSingleAssignment>(std::move(new_param_ref), std::move(node.return_variables[i]), node.tok);
			block_replace->add_as_stmt(std::move(node_assignment));
		}
		return node.replace_with(std::move(block_replace));
	}


};
