//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once


#include "ASTLowering.h"

/**
 * Lowering of single retain and single delete nodes into function calls to __incr__ and __decr__
 * Either for arrays or variables
 *
 */
class LoweringMemAlloc : public ASTLowering {

public:
	explicit LoweringMemAlloc(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeSingleRetain &node) override {
		// if composite type -> iterate
		if(node.ptr->ty->cast<CompositeType>()) {
			auto array_ref = static_cast<NodeCompositeRef*>(node.ptr.get());
			auto new_block = std::make_unique<NodeBlock>(node.tok);
			auto inner_block = array_ref->iterate_over(new_block, m_program);
			inner_block->prepend_as_stmt(get_ref_count_function_call(NodeStruct::INCREMENTOR, std::move(node.ptr), std::move(node.num)));
			return node.replace_with(std::move(new_block));
		}

		auto node_call = get_ref_count_function_call(NodeStruct::INCREMENTOR, std::move(node.ptr), std::move(node.num));
		return node.replace_with(std::move(node_call));
	}

	NodeAST * visit(NodeSingleDelete &node) override {
		if (node.destroy_instance) return lower_explicit_delete(node);
		// if composite type -> iterate
		if(node.ptr->ty->cast<CompositeType>()) {
			auto array_ref = static_cast<NodeCompositeRef*>(node.ptr.get());
			auto new_block = std::make_unique<NodeBlock>(node.tok);
			auto inner_block = array_ref->iterate_over(new_block, m_program);
			auto num = node.num ? std::move(node.num) : std::make_unique<NodeInt>(1, node.tok);
			inner_block->prepend_as_stmt(get_ref_count_function_call(NodeStruct::DECREMENTER, std::move(node.ptr), std::move(num)));
			return node.replace_with(std::move(new_block));
		}

		auto node_call = get_ref_count_function_call(NodeStruct::DECREMENTER, std::move(node.ptr), std::make_unique<NodeInt>(1, node.tok));
		return node.replace_with(std::move(node_call));
	}

	// Introduced after PointerScope: these snapshots borrow IDs without retaining them.
	std::unique_ptr<NodeReference> snapshot(NodeBlock& block, std::unique_ptr<NodeAST> value, Type* type, const Token& tok) const {
		auto var = m_program->get_tmp_var(type, DataType::Mutable, tok);
		auto ref = var->to_reference();
		ref->match_data_structure(var);
		block.add_as_stmt(std::make_unique<NodeSingleDeclaration>(std::move(var), std::move(value), tok));
		return ref;
	}

	NodeAST* lower_explicit_delete(NodeSingleDelete& node) const {
		auto block = std::make_unique<NodeBlock>(node.tok, true);
		if (node.ptr->ty->cast<CompositeType>()) {
			auto array = static_cast<NodeCompositeRef*>(node.ptr.get());
			auto body = array->iterate_over(block, m_program);
			auto del = std::make_unique<NodeSingleDelete>(std::move(node.ptr), std::make_unique<NodeInt>(1, node.tok), node.tok);
			del->destroy_instance = true;
			body->add_as_stmt(std::move(del));
			return node.replace_with(std::move(block));
		}

		// Preserve the destination before cloning it for the ID read and the nil assignment:
		//   delete events[next_index()]
		// must become (schematically):
		//   idx := next_index()
		//   old := events[idx]
		//   if old # nil: events[idx] := nil; __decr__(old, allocation[old])
		// Without idx, reading old and clearing the slot would each call next_index(),
		// potentially addressing different slots. Snapshot every dimension of an NDArray.
		//
		// ASTReturnFunctionRewriting runs later, after function strategy selection and
		// return-parameter promotion. Its hoister handles eligible calls in the single
		// snapshot assignment normally; calling it here would miss those prerequisites.
		// Hoisting after cloning the index would process two independent call nodes, not
		// deduplicate them. It also deliberately skips builtins and expression functions,
		// so it cannot replace capturing the complete index expression here.
		if (auto array = node.ptr->cast<NodeArrayRef>(); array and array->index) {
			array->index = snapshot(*block, std::move(array->index), TypeRegistry::Integer, node.tok);
		} else if (auto array = node.ptr->cast<NodeNDArrayRef>(); array and array->indexes) {
			for (auto& index : array->indexes->params)
				index = snapshot(*block, std::move(index), TypeRegistry::Integer, node.tok);
		}
		node.ptr->update_parents(&node);
		auto old = snapshot(*block, node.ptr->clone(), node.ptr->ty, node.tok);
		auto count = std::make_unique<NodeArrayRef>(
			node.ptr->ty->get_element_type()->ksp_encoded_string() + OBJ_DELIMITER + "allocation",
			old->clone(), node.tok);
		count->ty = TypeRegistry::Integer;
		auto body = std::make_unique<NodeBlock>(node.tok, true);
		// Clear before cascading destruction can invalidate a member used as the destination.
		body->add_as_stmt(std::make_unique<NodeSingleAssignment>(
			std::move(node.ptr), std::make_unique<NodeNil>(node.tok), node.tok));
		body->add_as_stmt(get_ref_count_function_call(NodeStruct::DECREMENTER,
			clone_as<NodeReference>(old.get()), std::move(count)));
		block->add_as_stmt(std::make_unique<NodeIf>(
			std::make_unique<NodeBinaryExpr>(token::NOT_EQUAL, std::move(old),
				std::make_unique<NodeNil>(node.tok), node.tok),
			std::move(body), std::make_unique<NodeBlock>(node.tok, true), node.tok));
		return node.replace_with(std::move(block));
	}

	/**
	 * creates call to any ref count function like __decr__, __incr__
	 */
	static std::unique_ptr<NodeFunctionCall> get_ref_count_function_call(const std::string& name, std::unique_ptr<NodeReference> arr, std::unique_ptr<NodeAST> num) {
		std::string func_name = arr->ty->get_element_type()->ksp_encoded_string()+OBJ_DELIMITER;
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
