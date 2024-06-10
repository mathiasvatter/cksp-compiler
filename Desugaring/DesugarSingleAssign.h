//
// Created by Mathias Vatter on 10.06.24.
//

#pragma once

#include "../Desugaring/ASTDesugaring.h"

/// class for lowering assign statements especially for array assignments -> get init function and replace with function call
class DesugarSingleAssign : public ASTDesugaring {
private:		
	inline void visit(NodeSingleAssignment& node) override {
		if(node.l_value->get_node_type() == NodeType::ArrayRef) {
			// if lhs is arrayref, check if array is initialized with a list of values
			if(node.r_value->get_node_type() != NodeType::ParamList) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
				error.m_message = "<Array> can only be assigned with a list of values.";
				error.m_expected = "<ParamList>";
				error.m_got = node.r_value->get_string();
				error.exit();
			}

			// if param list has only one value:
			auto param_list = static_cast<NodeParamList*>(node.r_value.get());
			auto node_array_ref = static_cast<NodeArrayRef*>(node.l_value.get());
			if(param_list->params.size() == 1) {
				DesugarSingleAssign::add_array_init_function_def(m_program, node.l_value->ty->get_element_type(), param_list->params[0].get());
				replacement_node = get_array_init_function_call(node_array_ref);
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

public:
	explicit DesugarSingleAssign(NodeProgram* program) : ASTDesugaring(program) {};

	static inline std::unique_ptr<NodeBody> get_array_init_function_call(NodeReference* array_ref) {
		auto node_iterator = std::make_unique<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, array_ref->tok);
		node_iterator->is_local = true;
		auto node_iterator_ref = node_iterator->to_reference();
		node_iterator_ref->match_data_structure(node_iterator.get());
		auto node_body = std::make_unique<NodeBody>(array_ref->tok);
		node_body->scope = true;

		auto node_function_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				"array.init."+array_ref->declaration->ty->get_element_type()->to_string(),
				std::make_unique<NodeParamList>(
					array_ref->tok,
					array_ref->clone(),
					std::move(node_iterator_ref)
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
	 * function array_init(array: [], iter: int)
	 *  while(iter < num_elements(array))
	 *      array[iter] := {type}
	 *      inc(iter)
	 *  end while
	 * end function
	 */
	static inline bool add_array_init_function_def(NodeProgram* program, Type* type, NodeAST* value) {
		std::string func_name = "array.init."+type->to_string();
		// check if function with this type already exists
		auto it = program->function_lookup.find({func_name, 2});
		if(it != program->function_lookup.end()) {
			return false;
		}
		std::unique_ptr<NodeAST> rhs_value = nullptr;
		if(!value) {
			rhs_value = TypeRegistry::get_neutral_element_from_type(type);
		} else {
			rhs_value = value->clone();
		}

		auto node_array = std::make_unique<NodeArray>(std::nullopt, "array", type, DataType::Array, nullptr, Token());
		auto node_iterator = std::make_unique<NodeVariable>(std::nullopt, "_iter", TypeRegistry::Integer, DataType::Mutable, Token());
		auto node_iterator_ref = node_iterator->to_reference();
		auto node_array_ref = std::make_unique<NodeArrayRef>("array", node_iterator->to_reference(), Token());
		auto node_function_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				std::make_unique<NodeParamList>(
					Token(),
					node_array->clone(),
					node_iterator->clone()
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

		auto node_while_body = std::make_unique<NodeBody>(Token());
		node_while_body->scope = true;
		auto node_assignment = std::make_unique<NodeSingleAssignment>(
			node_array_ref->clone(),
			std::move(rhs_value),
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
		node_function_def->body->add_stmt(std::make_unique<NodeStatement>(
			std::make_unique<NodeSingleAssignment>(
				node_iterator_ref->clone(),
				std::make_unique<NodeInt>(0, Token()),
				Token()),
			Token()));
		node_function_def->body->add_stmt(std::make_unique<NodeStatement>(std::move(new_while), Token()));
		program->function_definitions.push_back(std::move(node_function_def));
		// update function lookup so that the new function can be found
		program->update_function_lookup();
		return true;
	}


};