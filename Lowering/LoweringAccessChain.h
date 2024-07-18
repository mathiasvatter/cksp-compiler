//
// Created by Mathias Vatter on 18.07.24.
//

#pragma once

#include "ASTLowering.h"

/**
 *
 * List(42, nil){List}.next{List} -> List.next[List(42, nil)]
 * this_list{List}.next{List}.value{int} -> List.value[List.next[this_list]]
 * this_list{List}.next{List}.get_value() -> List.get_value(List.next[this_list])
 */

class LoweringAccessChain : public ASTLowering {
private:

public:
	explicit LoweringAccessChain(NodeProgram *program) : ASTLowering(program) {}

	inline void visit(NodeAccessChain& node) override {
		// if first element is pointer do not accept
		auto start = node.chain[0]->get_node_type() == NodeType::FunctionCall ? 0 : 1;
		for(int i=start; i<node.chain.size(); i++) {
			auto& ref = node.chain[i];
			ref->accept(*this);
		}

	}

	/// every pointer ref in access chain is replaced by array_ref with name "<obj_type>.<name>"
	inline void visit(NodePointerRef& node) override {
		if(node.ty == TypeRegistry::Unknown || node.ty->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "Unknown type for pointer reference. Pointer references have to be typed or of type <Object>.";
			error.m_got = node.ty->to_string();
			error.exit();
		}
		auto obj_name = node.ty->to_string();
		node.name = obj_name + "." + node.name;
		auto node_array = node.to_array_ref(nullptr);
		node_array->ty = TypeRegistry::ArrayOfInt;
		node.replace_with(std::move(node_array));
	}

	// increase dimensions -> to ndarray_ref
	inline void visit(NodeArrayRef& node) override {

	}

	inline void visit(NodeNDArrayRef& node) override {

	}

	inline void visit(NodeVariableRef& node) override {

	}

};
