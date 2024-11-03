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

		auto node_call = get_ref_count_function_call("__incr__", std::move(node.ptr), std::move(node.num));
		return node.replace_with(std::move(node_call));
	}

	NodeAST * visit(NodeSingleDelete &node) override {
		if(node.ty->get_element_type()->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "Delete can only be used with <Object> types.";
			error.exit();
		}

		auto node_call = get_ref_count_function_call("__decr__", std::move(node.ptr), std::make_unique<NodeInt>(1, node.tok));
		return node.replace_with(std::move(node_call));
	}

	/**
	 * creates call to any ref count function like __decr__, __incr__
	 * when ref is of composite type, it will call the array version of the function
	 */
	static std::unique_ptr<NodeFunctionCall> get_ref_count_function_call(const std::string& name, std::unique_ptr<NodeReference> arr, std::unique_ptr<NodeAST> num) {
		std::string func_name = arr->ty->get_element_type()->to_string()+OBJ_DELIMITER;
		if(arr->ty->get_type_kind() == TypeKind::Composite) {
			func_name += "Array"+OBJ_DELIMITER;
		}
		func_name += name;
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


};