//
// Created by Mathias Vatter on 10.06.24.
//

#pragma once

#include "../Desugaring/ASTDesugaring.h"

/// class for lowering assign statements especially for array assignments -> get init function and replace with function call
class DesugarSingleAssign : public ASTDesugaring {
private:
	inline void visit(NodeBody& node) override {
		for(auto &stmt : node.statements) {
			stmt->accept(*this);
		}
		node.cleanup_body();
	}
	inline void visit(NodeCallback& node) override {
		m_program->current_callback = &node;
		node.statements->accept(*this);
		m_program->current_callback = nullptr;
	}
	inline void visit(NodeSingleAssignment& node) override {
		if(node.l_value->get_node_type() == NodeType::ArrayRef) {
			auto node_array_ref = static_cast<NodeArrayRef*>(node.l_value.get());
			// if lhs is arrayref and has no index, check if array is initialized with a list of values
			if(!node_array_ref->index and node.r_value->get_node_type() != NodeType::ParamList) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "<Array> can only be assigned with a list of values.";
				error.m_expected = "<ParamList>";
				error.m_got = node.r_value->get_string();
				error.exit();
			} else if (node_array_ref->index and node.r_value->get_node_type() == NodeType::ParamList) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "Array element can only be assigned with a single value.";
				error.m_expected = "<Variable>";
				error.m_got = "<ParamList>";
				error.exit();
			} else if (node_array_ref->index) {
				return;
			}

			// if param list has only one value:
			auto param_list = static_cast<NodeParamList*>(node.r_value.get());
			if(param_list->params.size() == 1) {
				DesugarSingleAssign::add_array_init_function_def(m_program, node.l_value->ty->get_element_type());
				replacement_node = get_array_init_function_call(node_array_ref, param_list->params[0].get());
			} else {
				auto node_body = std::make_unique<NodeBody>(node.tok);
				for(int i = 0; i<param_list->params.size(); i++) {
					node_array_ref->index = std::make_unique<NodeInt>((int32_t)i, node.tok);
					auto &param = param_list->params[i];
					auto node_assign = std::make_unique<NodeSingleAssignment>(
						node_array_ref->clone(),
						param->clone(),
						param->tok
					);
					node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assign), param->tok));
				}
				replacement_node = std::move(node_body);
			}
		}
	}

	inline void visit(NodeSingleDeclaration& node) override {
		if(!node.variable->is_local) return;
		if(m_program->current_callback == m_program->init_callback) return;
		if(node.variable->get_node_type() == NodeType::Array) {
			auto node_body = std::make_unique<NodeBody>(node.tok);
			auto node_array = static_cast<NodeArray*>(node.variable.get());
			auto node_array_ref = node_array->to_reference();
			// if lhs is arrayref and has no index, check if array is initialized with a list of values
			if(node.value) {
				if (node.value->get_node_type() != NodeType::ParamList) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
					error.m_message = "<Array> can only be declared with a list of values.";
					error.m_expected = "<ParamList>";
					error.m_got = node.value->get_string();
					error.exit();
				}
			}

			if(!node.value) {
				DesugarSingleAssign::add_array_init_function_def(m_program, node.variable->ty->get_element_type());
				node_body->add_stmt(std::make_unique<NodeStatement>(
					get_array_init_function_call(node_array_ref.get(), TypeRegistry::get_neutral_element_from_type(node.variable->ty->get_element_type()).get()),
					node.tok));
			} else {
				// if param list has only one value:
				auto param_list = static_cast<NodeParamList *>(node.value.get());
				if (param_list->params.size() == 1) {
					DesugarSingleAssign::add_array_init_function_def(m_program, node.variable->ty->get_element_type());
					node_body->add_stmt(std::make_unique<NodeStatement>(
						get_array_init_function_call(node_array_ref.get(), param_list->params[0].get()),
						node.tok));
				} else {
					auto array_ref = static_cast<NodeArrayRef*>(node_array_ref.get());
					for (int i = 0; i < param_list->params.size(); i++) {
						array_ref->index = std::make_unique<NodeInt>((int32_t) i, node.tok);
						auto &param = param_list->params[i];
						auto node_assign = std::make_unique<NodeSingleAssignment>(
							node_array_ref->clone(),
							param->clone(),
							param->tok
						);
						node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assign), param->tok));
					}
				}
			}
			node_body->prepend_stmt(std::make_unique<NodeStatement>(node.clone(), node.tok));
			node.replace_with(std::move(node_body));
		}
	}

public:
	explicit DesugarSingleAssign(NodeProgram* program) : ASTDesugaring(program) {};

	static inline std::unique_ptr<NodeBody> get_array_init_function_call(NodeReference* array_ref, NodeAST* value) {
		std::string func_name = "array.init."+array_ref->declaration->ty->get_element_type()->to_string();
		auto node_iterator = std::make_unique<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, array_ref->tok);
		node_iterator->is_local = true;
		auto node_iterator_ref = node_iterator->to_reference();
		node_iterator_ref->match_data_structure(node_iterator.get());
		auto node_body = std::make_unique<NodeBody>(array_ref->tok);
		node_body->scope = true;

		std::unique_ptr<NodeAST> rhs_value = nullptr;
		if(!value) {
			rhs_value = TypeRegistry::get_neutral_element_from_type(array_ref->declaration->ty->get_element_type());
		} else {
			rhs_value = value->clone();
		}

		auto node_function_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeParamList>(
					array_ref->tok,
					array_ref->clone(),
					std::move(node_iterator_ref),
					std::move(rhs_value)
				),
				array_ref->tok
			),
			array_ref->tok
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(
			std::make_unique<NodeSingleDeclaration>(
				std::move(node_iterator),
				nullptr, array_ref->tok
			),
			array_ref->tok)
		);
		node_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_function_call), array_ref->tok));
		return node_body;
	}

	/**
	 * function array_init(array: [], iter: int, value: {type})
	 *  while(iter < num_elements(array))
	 *      array[iter] := {type}
	 *      inc(iter)
	 *  end while
	 * end function
	 */
	static inline bool add_array_init_function_def(NodeProgram* program, Type* type) {
		std::string func_name = "array.init."+type->to_string();
		// check if function with this type already exists
		auto it = program->function_lookup.find({func_name, 3});
		if(it != program->function_lookup.end()) {
			return false;
		}

		auto node_array = std::make_unique<NodeArray>(std::nullopt, "array", type, nullptr, Token());
		auto node_iterator = std::make_unique<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, Token());
		auto node_value = std::make_unique<NodeVariable>(std::nullopt, "value", type, DataType::Mutable, Token());
		auto node_iterator_ref = node_iterator->to_reference();
		auto node_array_ref = std::make_unique<NodeArrayRef>("array", node_iterator->to_reference(), Token());
		auto node_value_ref = node_value->to_reference();
		auto node_function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeParamList>(
					Token(),
					node_array->clone(),
					node_iterator->clone(),
					node_value->clone()
				),
				Token()
			),
			std::nullopt,
			false,
			std::make_unique<NodeBody>(Token()),
			Token()
		);
		node_function_def->body->scope = true;

		// get declaration pointer right
		node_array_ref->match_data_structure(static_cast<NodeDataStructure*>(node_function_def->header->args->params[0].get()));
		node_iterator_ref->match_data_structure(static_cast<NodeDataStructure*>(node_function_def->header->args->params[1].get()));
		node_value_ref->match_data_structure(static_cast<NodeDataStructure*>(node_function_def->header->args->params[2].get()));

		auto node_while_body = std::make_unique<NodeBody>(Token());
		node_while_body->scope = true;
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			node_array_ref->clone(),
			std::move(node_value_ref),
			Token()
		);
		auto node_inc = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"inc",
				std::make_unique<NodeParamList>(Token(),node_iterator_ref->clone()),Token()),
			Token()
		);

		node_while_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_assignment), Token()));
		node_while_body->add_stmt(std::make_unique<NodeStatement>(std::move(node_inc), Token()));

		// delete index of node_array_ref for its last usage in num_elements
		node_array_ref->index = nullptr;
		auto new_while = std::make_unique<NodeWhile>(
			std::make_unique<NodeBinaryExpr>(
				token::LESS_THAN,
				node_iterator_ref->clone(),
				std::make_unique<NodeFunctionCall>(
					false,
					std::make_unique<NodeFunctionHeader>(
						"num_elements",
						std::make_unique<NodeParamList>(Token(),node_array_ref->clone()),Token()),
					Token()
				),
				Token()
			),
			std::move(node_while_body),
			Token()
		);
//		node_function_def->body->add_stmt(std::make_unique<NodeStatement>(
//			std::make_unique<NodeSingleAssignment>(
//				node_iterator_ref->clone(),
//				std::make_unique<NodeInt>(0, Token()),
//				Token()),
//			Token()));
		node_function_def->body->add_stmt(std::make_unique<NodeStatement>(std::move(new_while), Token()));
		program->function_definitions.push_back(std::move(node_function_def));
		// update function lookup so that the new function can be found
		program->update_function_lookup();
		return true;
	}


};