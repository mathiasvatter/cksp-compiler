//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once


#include "ASTLowering.h"

class LoweringNumElements : public ASTLowering {
private:

public:
	explicit LoweringNumElements(NodeProgram *program) : ASTLowering(program) {}


	NodeAST * visit(NodeNumElements &node) override {
		if(node.ty->get_type_kind() != TypeKind::Composite) {
			auto error = CompileError(ErrorType::TypeError, "", "", node.tok);
			error.m_message = "<num_elements> can only be used with <Composite> types like <Arrays> or <NDArrays>.";
			error.exit();
		}

		// if no dimension is given and it is ndarray -> use 0
		if(node.get_node_type() == NodeType::NDArrayRef and !node.dimension) {
			node.set_dimension(std::make_unique<NodeInt>(0, node.tok));
		}

		if(node.get_node_type() == NodeType::NDArrayRef) {
			auto nd_array = static_cast<NodeNDArrayRef*>(node.array.get());
			auto array_call = std::make_unique<NodeArrayRef>(
				nd_array->name + OBJ_DELIMITER + "num_elements",
				std::move(node.dimension),
				node.tok
			);
			return node.replace_with(std::move(array_call));
		}

		return node.replace_with(DefinitionProvider::num_elements(std::move(node.array)));
	}

};