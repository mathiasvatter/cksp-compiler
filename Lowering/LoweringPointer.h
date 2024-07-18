//
// Created by Mathias Vatter on 07.07.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringPointer : public ASTLowering {
private:


public:
	explicit LoweringPointer(NodeProgram *program) : ASTLowering(program) {}

	inline void visit(NodePointer& node) override {
//		auto node_var = node.to_variable();
//		node_var->ty = TypeRegistry::Integer;
//		node.replace_with(std::move(node_var));
		node.lower_type();
	}

	inline void visit(NodePointerRef& node) override {
		if(node.ty == TypeRegistry::Unknown || node.ty->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "Unknown type for pointer reference. Pointer references have to be typed or of type <Object>.";
			error.m_got = node.ty->to_string();
			error.exit();
		}

		// check if parent string -> call __repr__ method
		auto obj_name = node.ty->to_string();
		static auto func_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeader>(
				obj_name+".__repr__",
				 std::make_unique<NodeParamList>(node.tok, node.clone()),
				 node.tok
			 	),
			node.tok
			);
		node.lower_type();

		if(node.is_string_repr()) {
			node.replace_with(std::move(func_call));
			return;
		}

	}
};