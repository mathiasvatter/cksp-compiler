//
// Created by Mathias Vatter on 03.11.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringUseCount : public ASTLowering {
private:

public:
	explicit LoweringUseCount(NodeProgram *program) : ASTLowering(program) {}

	NodeAST *visit(NodeUseCount &node) override {
		if (node.ref->ty->get_element_type()->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "<use_count> can only be used with <Object> types.";
			error.exit();
		}
		if(node.ref->ty->get_type_kind() == TypeKind::Composite) {
			auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
			error.m_message = "<use_count> can not be used with <Composite> types.";
		}


		std::string prefix = node.ref->ty->to_string();
		auto nil_check = make_nil_check(clone_as<NodeReference>(node.ref.get()));
		auto node_arr_ref = std::make_unique<NodeArrayRef>(
			prefix+OBJ_DELIMITER+"allocation", std::move(node.ref), node.tok
		);
//		nil_check->if_body->add_
//
		return node.replace_with(std::move(node_arr_ref));
	}
};