//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once


#include "ASTLowering.h"

class LoweringRetain : public ASTLowering {
private:

public:
	explicit LoweringRetain(NodeProgram *program) : ASTLowering(program) {}


	NodeAST * visit(NodeSingleRetain &node) override {
		if(node.ty->get_element_type()->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "Retain can only be used with <Object> types.";
			error.exit();
		}

		if(node.ty->get_type_kind() == TypeKind::Composite) {
			add_array_incr_function(node.ty->get_element_type(), "__incr__", m_program);
			auto node_call = get_array_incr_function_call("__incr__", std::move(node.ptr), std::move(node.num));

			return node.replace_with(std::move(node_call));
		}

		auto func_name = node.ty->get_element_type()->to_string()+OBJ_DELIMITER+"__incr__";
		auto incr_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				std::make_unique<NodeParamList>(node.tok, node.ptr->clone(), std::move(node.num)),
				node.tok
			),
			node.tok
		);
		return node.replace_with(std::move(incr_call));
	}

	std::unique_ptr<NodeFunctionCall> get_array_incr_function_call(const std::string& name, std::unique_ptr<NodeReference> arr, std::unique_ptr<NodeAST> num) {
		auto func_name = arr->ty->get_element_type()->to_string()+OBJ_DELIMITER+"Array"+OBJ_DELIMITER+name;
		return std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				std::make_unique<NodeParamList>(arr->tok, std::move(arr), std::move(num)),
				Token()
			),
			Token()
		);
	}

	/**
	 * function Point::Array::__incr__(arr: int[], num: int)
	 * 	declare i: int
	 * 	for i := 0 to num_elements(arr)-1
	 * 		Point::__incr__(arr[i], num)
	 * 	end for
	 * end function
	 * @param ty
	 * @return
	 */
	static inline bool add_array_incr_function(Type* ty, const std::string& name, NodeProgram* program) {
		auto func_name = ty->to_string()+OBJ_DELIMITER+"Array"+OBJ_DELIMITER+name;
		auto func_call_name = ty->to_string()+OBJ_DELIMITER+name;

		if(program->function_lookup.find({func_name, 2}) != program->function_lookup.end()) {
			return false;
		}

		auto arr = std::make_shared<NodeArray>(std::nullopt, "arr", TypeRegistry::ArrayOfInt, nullptr, Token());
		auto num = std::make_shared<NodeVariable>(std::nullopt, "num", TypeRegistry::Integer, DataType::Mutable, Token());
		auto iterator = get_iterator_var(Token());
		auto arr_ref = unique_ptr_cast<NodeArrayRef>(arr->to_reference());
		auto num_ref = num->to_reference();
		arr_ref->set_index(iterator->to_reference());
		auto incr_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_call_name,
				std::make_unique<NodeParamList>(arr->tok, arr_ref->clone(), std::move(num_ref)),
				arr->tok
			),
			arr->tok
		);

		auto func_def = std::make_unique<NodeFunctionDefinition>(
			std::make_unique<NodeFunctionHeader>(
				func_name,
				Token(),
				std::move(arr),
				std::move(num)
			),
			std::nullopt,
			false,
			std::make_unique<NodeBlock>(Token(), true),
			Token()
		);
		func_def->body->add_as_stmt(std::move(incr_call));
		func_def->body->wrap_in_loop(iterator, std::make_unique<NodeInt>(0, Token()), arr_ref->get_size());
		func_def->body->get_last_statement()->desugar(program);
		func_def->ty = TypeRegistry::Void;
		func_def->header->create_function_type(TypeRegistry::Void);

		program->additional_function_definitions.push_back(std::move(func_def));
		program->update_function_lookup();
		return true;

	}

};