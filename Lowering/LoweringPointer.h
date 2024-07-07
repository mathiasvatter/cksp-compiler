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
		auto node_var = node.to_variable();
		node_var->ty = TypeRegistry::Integer;
		node.replace_with(std::move(node_var));
	}
	inline void visit(NodePointerRef& node) override {
		if(!node.is_valid_object_type(m_program)) {
			auto error = CompileError(ErrorType::TypeError, "Pointer reference must be a valid object type.", "", node.tok);
			error.exit();
		}
		auto obj = node.ty->to_string();
		auto obj_index = node.to_variable_ref();
		auto array_ref = node.to_array_ref(obj_index.get());
		array_ref->ty = TypeRegistry::ArrayOfInt;


	}
};