//
// Created by Mathias Vatter on 03.11.24.
//

#pragma once

#include "ASTLowering.h"
#include "PreLoweringStruct.h"

/**
 * Pre Lowering:
 * message(use_count(pointer))
 *
 * Post Lowering:
 * message(List::allocation[Struct::max(0,pointer)])
 */
class LoweringUseCount final : public ASTLowering {

public:
	explicit LoweringUseCount(NodeProgram *program) : ASTLowering(program) {}

	NodeAST *visit(NodeUseCount &node) override {
		if (node.ref->ty->get_element_type()->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "<use_count> can only be used with <Object> types. "+node.ref->tok.val+ " has to be a <Pointer> to an <Object> instance.";
			error.exit();
		}
		if(node.ref->ty->get_type_kind() == TypeKind::Composite) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "<use_count> can not be used with <Composite> types. Use single <Array> or <NDArray> elements instead.";
		}

		std::string prefix = node.ref->ty->to_string();
		PreLoweringStruct::add_max_min_function_defs(m_program, false);
		auto node_call = PreLoweringStruct::create_min_max_call(std::make_unique<NodeInt>(0, node.tok), std::move(node.ref), false);
		auto node_arr_ref = std::make_unique<NodeArrayRef>(
			prefix+OBJ_DELIMITER+"allocation", std::move(node_call), node.tok
		);

		return node.replace_with(std::move(node_arr_ref));
	}
};