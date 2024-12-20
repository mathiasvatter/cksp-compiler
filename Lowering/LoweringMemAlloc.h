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
private:

public:
	explicit LoweringMemAlloc(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeSingleRetain &node) override {
		// if composite type -> iterate
		if(node.ptr->ty->cast<CompositeType>()) {
			auto array_ref = static_cast<NodeCompositeRef*>(node.ptr.get());
			auto new_block = std::make_unique<NodeBlock>(node.tok);
			auto inner_block = array_ref->iterate_over(new_block, m_program);
			inner_block->prepend_as_stmt(get_ref_count_function_call("__incr__", std::move(node.ptr), std::move(node.num)));
			return node.replace_with(std::move(new_block));
		}

		auto node_call = get_ref_count_function_call("__incr__", std::move(node.ptr), std::move(node.num));
		return node.replace_with(std::move(node_call));
	}

	NodeAST * visit(NodeSingleDelete &node) override {
		// if composite type -> iterate
		if(node.ptr->ty->cast<CompositeType>()) {
			auto array_ref = static_cast<NodeCompositeRef*>(node.ptr.get());
			auto new_block = std::make_unique<NodeBlock>(node.tok);
			auto inner_block = array_ref->iterate_over(new_block, m_program);
			inner_block->prepend_as_stmt(get_ref_count_function_call("__decr__", std::move(node.ptr), std::move(node.num)));
			return node.replace_with(std::move(new_block));
		}

		auto node_call = get_ref_count_function_call("__decr__", std::move(node.ptr), std::make_unique<NodeInt>(1, node.tok));
		return node.replace_with(std::move(node_call));
	}

	/**
	 * creates call to any ref count function like __decr__, __incr__
	 */
	static std::unique_ptr<NodeFunctionCall> get_ref_count_function_call(const std::string& name, std::unique_ptr<NodeReference> arr, std::unique_ptr<NodeAST> num) {
		std::string func_name = arr->ty->get_element_type()->to_string()+OBJ_DELIMITER;
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