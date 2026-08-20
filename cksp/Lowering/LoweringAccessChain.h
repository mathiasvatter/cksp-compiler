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

class LoweringAccessChain final : public ASTLowering {
	NodeAST* start_pointer = nullptr;
	Type* prev_type = nullptr;

	/// <static> members are shared by all instances -> they are never expanded by an instance index
	static NodeReference* get_shared_member_ref(NodeAST* node) {
		const auto ref = node->is_reference();
		if(!ref) return nullptr;
		const auto declaration = ref->get_declaration();
		return declaration and declaration->is_shared_member() ? ref : nullptr;
	}

	/// <static function>: called on the struct, so the preceding chain element is not a receiver
	static bool is_static_method_call(NodeAST* node) {
		const auto func_call = node->cast<NodeFunctionCall>();
		if(!func_call) return false;
		const auto definition = func_call->get_definition();
		return definition and definition->is_static;
	}
public:
	explicit LoweringAccessChain(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeAccessChain& node) override {
		node.update_types();
		// if first element is pointer do not accept
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
			// <static> members and methods belong to the struct, not to an instance: the preceding
			// element is not an index or receiver and is dropped. Only safe for side-effect free
			// receivers, since a call would be lost entirely.
			const auto shared_ref = get_shared_member_ref(curr_node.get());
			if(shared_ref or is_static_method_call(curr_node.get())) {
				if(auto func_call = prev_node->cast<NodeFunctionCall>()) {
					const std::string what = shared_ref
						? "<static> member <"+shared_ref->name+">"
						: "<static function> <"+curr_node->cast<NodeFunctionCall>()->function->name+">";
					auto error = make_diagnostic(ErrorType::SyntaxError, *curr_node);
					error.message = what+" cannot be accessed on a temporary object. It does not depend on the "
						"instance, so the object would never be created. Assign the result to a variable first.";
					error.exit();
				}
				continue;
			}
			if(auto node_array_ref = curr_node->cast<NodeArrayRef>()) {
				node_array_ref->set_index(std::move(prev_node));
			} else if(auto node_ndarray_ref = curr_node->cast<NodeNDArrayRef>()) {
				node_ndarray_ref->indexes->prepend_param(std::move(prev_node));
			} else if(auto node_func_call = curr_node->cast<NodeFunctionCall>()) {
				node_func_call->function->prepend_arg(std::move(prev_node));
			}
		}
		node.chain.back()->collect_references();
		return node.replace_with(std::move(node.chain.back()));
	}

	/// every pointer ref in access chain is replaced by array_ref with name "<obj_type>.<name>"
	NodeAST * visit(NodePointerRef& node) override {
		if(node.ty == TypeRegistry::Unknown || node.ty->get_type_kind() != TypeKind::Object) {
			auto error = Diagnostic(ErrorType::TypeError, "", "", node.tok);
			error.message = "Unknown type for pointer reference. Pointer references have to be typed or of type <Object>.";
			error.actual = node.ty->to_string();
			error.exit();
		}
		if(&node == start_pointer) return &node;
		node.name = prev_type->ksp_encoded_string() + OBJ_DELIMITER + node.name;
		// <static> members are shared -> stay a plain pointer, no instance index
		if(get_shared_member_ref(&node)) return &node;
		auto node_array = node.expand_dimension(nullptr);
		node_array->collect_references();
		return node.replace_reference(std::move(node_array));
	}

	// increase dimensions -> to ndarray_ref
	NodeAST * visit(NodeArrayRef& node) override {
		if(&node == start_pointer) return &node;
		// no index -> array -> List.array[sth, *]
//		if(!node.index) node.set_index(std::make_unique<NodeWildcard>("*", node.tok));
		node.name = prev_type->ksp_encoded_string()+OBJ_DELIMITER+node.name;
		// <static const> members are shared -> stay a plain array ref, no extra dimension
		if(get_shared_member_ref(&node)) return &node;
		auto node_ndarray_ref = node.expand_dimension(nullptr);
		node_ndarray_ref->collect_references();
		return node.replace_reference(std::move(node_ndarray_ref));
	}

	NodeAST * visit(NodeNDArrayRef& node) override {
		if(&node == start_pointer) return &node;
		node.name = prev_type->ksp_encoded_string()+OBJ_DELIMITER+node.name;
		if(get_shared_member_ref(&node)) {
			// no extra dimension, but the reference still needs its own dimensions to lower its
			// index later - for a per-instance member expand_dimension() determines them on the way
			node.determine_sizes();
			return &node;
		}
		auto node_ndarray_ref = node.expand_dimension(nullptr);
		node_ndarray_ref->collect_references();
		return &node;
	}

	NodeAST * visit(NodeVariableRef& node) override {
		if(&node == start_pointer) return &node;
		node.name = prev_type->ksp_encoded_string()+OBJ_DELIMITER+node.name;
		// <static const> members are shared -> stay a plain reference, no index
		if(get_shared_member_ref(&node)) return &node;
		auto node_array_ref = node.expand_dimension(nullptr);
		node_array_ref->collect_references();
		return node.replace_reference(std::move(node_array_ref));
	}

	NodeAST * visit(NodeFunctionCall& node) override {
		if(&node == start_pointer) return &node;
		node.function->name = prev_type->ksp_encoded_string()+OBJ_DELIMITER+node.function->name;
		node.bind_definition(m_program);
		return &node;
	}

};
