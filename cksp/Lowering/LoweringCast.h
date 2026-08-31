//
// Created by Mathias Vatter on 31.08.26.
//

#pragma once

#include "ASTLowering.h"

class LoweringCast final : public ASTLowering {

public:
	explicit LoweringCast(NodeProgram *program) : ASTLowering(program) {}

	NodeAST* visit(NodeCast& node) override {
		const auto element_type = node.target_type->get_element_type();
		// if node.value already has same type as cast -> replace with node.value
		if (node.value->ty == element_type) {
			return node.replace_with(std::move(node.value));
		}

		// if casted to an object type -> switch node.value type to int and replace node with node.value
		if (element_type->cast<ObjectType>()) {
			node.value->do_type_lowering(m_program);
			return node.replace_with(std::move(node.value));
		}

		std::unique_ptr<NodeFunctionCall> func_call = nullptr;
		if (element_type == TypeRegistry::Integer) {
			func_call = DefinitionProvider::create_builtin_call("int", std::move(node.value));
		} else if (element_type == TypeRegistry::Real) {
			func_call = DefinitionProvider::create_builtin_call("real", std::move(node.value));
			func_call->ty = TypeRegistry::Real;
		} else if (element_type == TypeRegistry::Boolean) {
			// boolean function gets further lowered in LoweringFunctionCall
			func_call = DefinitionProvider::create_builtin_call("bool", std::move(node.value));
		} else if (element_type == TypeRegistry::String) {
			// ksp automatically casts real and int to string so that they can get used in
			// string contexts. since boolean and object are basically int, nothing needs to be done here as well
		}

		func_call->bind_definition(m_program);
		func_call->collect_references();

		return node.replace_with(std::move(func_call));
	}

};