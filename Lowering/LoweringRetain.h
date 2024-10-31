//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once


#include "ASTLowering.h"

class LoweringRetain : public ASTLowering {
private:

public:
	explicit LoweringRetain(NodeProgram *program) : ASTLowering(program) {}


	NodeAST * visit(NodeSingleRetain &node) override {
		if(node.ty->get_element_type()->get_type_kind() != TypeKind::Object) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "Retain can only be used with <Object> types.";
			error.exit();
		}
		auto func_name = node.ty->get_element_type()->to_string()+OBJ_DELIMITER+"__incr__";
		if(node.ty->get_type_kind() == TypeKind::Composite) {

			auto comp_node = cast_node<NodeCompositeRef>(node.ptr.get());
			auto size = comp_node->get_size();




		}

		auto incr_call = std::make_unique<NodeFunctionCall>(
			false,
			std::make_unique<NodeFunctionHeaderRef>(
				func_name,
				std::make_unique<NodeParamList>(node.tok, std::move(node.ptr), std::move(node.num)),
				node.tok
			),
			node.tok
		);
		return node.replace_with(std::move(incr_call));
	}

};