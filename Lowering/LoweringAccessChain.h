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
	/// saves for each chain node its replacement slot, that the next is going into
	std::map<NodeAST*, NodeAST*> node_replacement_slot;
public:
	explicit LoweringAccessChain(NodeProgram *program) : ASTLowering(program) {}

	inline void visit(NodeAccessChain& node) override {
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
			if(curr_node->get_node_type() == NodeType::ArrayRef) {
				auto node_array_ref = static_cast<NodeArrayRef*>(curr_node.get());
				node_array_ref->index = std::move(prev_node);
			} else if(curr_node->get_node_type() == NodeType::NDArrayRef) {
				auto node_ndarray_ref = static_cast<NodeNDArrayRef*>(curr_node.get());
				node_ndarray_ref->indexes->prepend_param(std::move(prev_node));
			} else if(curr_node->get_node_type() == NodeType::FunctionCall) {
				auto node_func_call = static_cast<NodeFunctionCall*>(curr_node.get());
				node_func_call->function->args->prepend_param(std::move(prev_node));
			}
		}
		lowered_node = node.chain.back().get();
	}

	/// every pointer ref in access chain is replaced by array_ref with name "<obj_type>.<name>"
	inline void visit(NodePointerRef& node) override {
		if(node.ty == TypeRegistry::Unknown || node.ty->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "Unknown type for pointer reference. Pointer references have to be typed or of type <Object>.";
			error.m_got = node.ty->to_string();
			error.exit();
		}
//		node.lower_type();
		if(&node == start_pointer) return;
		node.name = prev_type->to_string() + "." + node.name;
		auto node_array = node.to_array_ref(nullptr);
		node_array->ty = TypeRegistry::ArrayOfInt;
		node.replace_with(std::move(node_array));
	}

	// increase dimensions -> to ndarray_ref
	inline void visit(NodeArrayRef& node) override {
//		node.lower_type();
		if(&node == start_pointer) return;
		// no index -> array -> List.array[sth, *]
		if(!node.index) node.index = std::make_unique<NodeWildcard>("*", node.tok);
		auto node_ndarray_ref = node.to_ndarray_ref();
		node_ndarray_ref->name = prev_type->to_string()+"."+node.name;
		node_ndarray_ref->declaration = node.declaration;
		node_ndarray_ref->determine_sizes();
		node_ndarray_ref->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type(), node_ndarray_ref->sizes->params.size());
		node.replace_with(std::move(node_ndarray_ref));
	}

	inline void visit(NodeNDArrayRef& node) override {
//		node.lower_type();
		if(&node == start_pointer) return;
		node.determine_sizes();
		// if array has no indexes -> everything should be copied -> wildcards for every index of size
		if (!node.indexes) {
			node.indexes = std::make_unique<NodeParamList>(node.tok);
			for (int i = 1; i < node.sizes->params.size(); i++) {
				node.indexes->add_param(std::make_unique<NodeWildcard>("*", node.tok));
			}
		}
		node.name = prev_type->to_string()+"."+node.name;
	}

	inline void visit(NodeVariableRef& node) override {
//		node.lower_type();
		if(&node == start_pointer) return;
		auto node_array_ref = node.to_array_ref(nullptr);
		node_array_ref->name = prev_type->to_string()+"."+node.name;
		node_array_ref->ty = node.ty;
		node_array_ref->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type());
		node.replace_with(std::move(node_array_ref));
	}

	inline void visit(NodeFunctionCall& node) override {
//		if(node.ty->get_element_type()->get_type_kind() == TypeKind::Object) {
//			node.set_element_type(TypeRegistry::Integer);
//		}
		if(&node == start_pointer) return;
		node.function->name = prev_type->to_string()+"."+node.function->name;
	}

};
