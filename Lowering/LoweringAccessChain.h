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
	NodeAST* start_pointer = nullptr;
	Type* prev_type = nullptr;
public:
	explicit LoweringAccessChain(NodeProgram *program) : ASTLowering(program) {}

	inline NodeAST * visit(NodeAccessChain& node) override {
		node.update_types();
		// if first element is pointer do not accept
//		auto start = node.chain[0]->get_node_type() == NodeType::FunctionCall ? 0 : 1;
		start_pointer = node.chain[0].get();
		for(int i=0; i<node.chain.size(); i++) {
			auto& ref = node.chain[i];
			ref->accept(*this);
			prev_type = node.types[i];
		}

		// create array nest
		// this_list{int}.List.next{int[]}.List.value{int[]}
		// -> List.value[List.next[this_list]]
		for(int i=1; i<node.chain.size(); i++) {
			auto& prev_node = node.chain[i-1];
			auto& curr_node = node.chain[i];
			if(auto node_array_ref = curr_node->cast<NodeArrayRef>()) {
				node_array_ref->set_index(std::move(prev_node));
			} else if(auto node_ndarray_ref = curr_node->cast<NodeNDArrayRef>()) {
				node_ndarray_ref->indexes->set_param(0, std::move(prev_node));
			} else if(auto node_func_call = curr_node->cast<NodeFunctionCall>()) {
				node_func_call->function->prepend_arg(std::move(prev_node));
			}
		}
		return node.replace_with(std::move(node.chain.back()));
	}

	/// every pointer ref in access chain is replaced by array_ref with name "<obj_type>.<name>"
	inline NodeAST * visit(NodePointerRef& node) override {
		if(node.ty == TypeRegistry::Unknown || node.ty->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "Unknown type for pointer reference. Pointer references have to be typed or of type <Object>.";
			error.m_got = node.ty->to_string();
			error.exit();
		}
		if(&node == start_pointer) return &node;
		node.name = prev_type->to_string() + OBJ_DELIMITER + node.name;
		auto node_array = node.inflate_dimension(nullptr);
//		auto node_array = node.to_array_ref(nullptr);
//		node_array->declaration = node.declaration;
//		node_array->ty = node.ty;
		return node.replace_reference(std::move(node_array));
	}

	// increase dimensions -> to ndarray_ref
	inline NodeAST * visit(NodeArrayRef& node) override {
		if(&node == start_pointer) return &node;
		// no index -> array -> List.array[sth, *]
//		if(!node.index) node.set_index(std::make_unique<NodeWildcard>("*", node.tok));
		node.name = prev_type->to_string()+OBJ_DELIMITER+node.name;
		auto node_ndarray_ref = node.inflate_dimension(nullptr);
//		auto node_ndarray_ref = node.to_ndarray_ref();
//		node_ndarray_ref->declaration = node.declaration;
//		node_ndarray_ref->determine_sizes();
//		node_ndarray_ref->ty = node.ty;
		return node.replace_reference(std::move(node_ndarray_ref));
	}

	inline NodeAST * visit(NodeNDArrayRef& node) override {
		if(&node == start_pointer) return &node;
		node.name = prev_type->to_string()+OBJ_DELIMITER+node.name;
		auto node_ndarray_ref = node.inflate_dimension(nullptr);
		return &node;
	}

	inline NodeAST * visit(NodeVariableRef& node) override {
		if(&node == start_pointer) return &node;
		node.name = prev_type->to_string()+OBJ_DELIMITER+node.name;
		auto node_array_ref = node.inflate_dimension(nullptr);
//		auto node_array_ref = node.to_array_ref(nullptr);
//		node_array_ref->declaration = node.declaration;
//		node_array_ref->ty = node.ty;
		return node.replace_reference(std::move(node_array_ref));
	}

	inline NodeAST * visit(NodeFunctionCall& node) override {
		if(&node == start_pointer) return &node;
		node.function->name = prev_type->to_string()+OBJ_DELIMITER+node.function->name;
		node.bind_definition(m_program);
		return &node;
	}

};
