//
// Created by Mathias Vatter on 17.06.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringStruct : public ASTLowering {
private:
	NodeStruct* m_current_struct = nullptr;
	NodeVariable* m_free_idx_var = nullptr;
	NodeArray* m_allocation_var = nullptr;
public:
	explicit LoweringStruct(NodeProgram *program) : ASTLowering(program) {}

	void visit(NodeStruct& node) override {
		m_current_struct = &node;

		// check if all member node types are allowed
		for(auto & m: node.member_table) {
			if(NodeStruct::allowed_member_node_types.find(m.second->get_node_type()) == NodeStruct::allowed_member_node_types.end()) {
				auto error = CompileError(ErrorType::SyntaxError, "", "", m.second->tok);
				error.m_message = "Member type not allowed in struct. Only <Variables>, <Arrays>, <NDArrays>, <Pointers> and <Structs> are allowed.";
				error.m_got = m.first;
				error.exit();
			}
			node.member_node_types.insert(m.second->get_node_type());
		}

		node.members->accept(*this);
		node.members->prepend_body(add_compiler_struct_vars());
		for(auto & m: node.methods) {
			m->accept(*this);
		}
	}

	std::unique_ptr<NodeBlock> add_compiler_struct_vars() {
		auto node_block = std::make_unique<NodeBlock>(Token());
		// add free_idx variable and allocation array to struct
		auto free_idx_var = std::make_unique<NodeVariable>(
			std::nullopt,
			m_current_struct->name+".free_idx",
			TypeRegistry::Integer,
			DataType::Mutable,
			Token()
		);
		auto free_idx_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(free_idx_var),
			std::make_unique<NodeInt>(0, Token()),
			Token()
		);
		m_free_idx_var = static_cast<NodeVariable*>(free_idx_decl->variable.get());
		auto allocation_var = std::make_unique<NodeArray>(
			std::nullopt,
			m_current_struct->name+".allocation",
			TypeRegistry::add_composite_type(CompoundKind::Array, TypeRegistry::Integer),
			NodeStruct::max_structs_var->to_reference(),
			Token()
		);
		auto allocation_decl = std::make_unique<NodeSingleDeclaration>(
			std::move(allocation_var),
			nullptr,
			Token()
		);
		m_allocation_var = static_cast<NodeArray*>(allocation_decl->variable.get());
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(free_idx_decl), Token()));
		node_block->add_stmt(std::make_unique<NodeStatement>(std::move(allocation_decl), Token()));
		return node_block;
	}

	inline void visit(NodeSingleDeclaration& node) override {
		// turn member into array if it is a member
		node.variable->accept(*this);
		if(node.value) node.value->accept(*this);

		// turn node.value into param list if is member and has been turned into array
		if(node.variable->is_member() and node.value) {
			if(node.variable->get_node_type() != NodeType::Array and node.variable->get_node_type() != NodeType::NDArray) {
				return;
			}
			if(node.value->get_node_type() != NodeType::ParamList) {
				auto param_list = std::make_unique<NodeParamList>(node.tok);
				param_list->params.push_back(std::move(node.value));
				param_list->parent = &node;
				node.value = std::move(param_list);
			}
		}
	}

	inline void visit(NodeVariable& node) override {
		// if member, turn into array
		if(node.is_member()) {
			auto node_array = node.to_array(NodeStruct::max_structs_var->to_reference().get());
			node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type());
			node.replace_with(std::move(node_array));
		}
	}
	inline void visit(NodePointer& node) override {
		// if member, turn into array of pointers
		if(node.is_member()) {
			auto node_array = node.to_array(NodeStruct::max_structs_var->to_reference().get());
			node_array->ty = TypeRegistry::add_composite_type(CompoundKind::Array, node.ty->get_element_type());
			node.replace_with(std::move(node_array));
		}
	}
	inline void visit(NodeArray& node) override {
		// if member, turn into multi-dimensional array
		/*
		 * 	declare velocities[10]: [int]
		 * 	declare struct.velocity[MAX_STRUCTS/10, 10]
		 */

	}


};