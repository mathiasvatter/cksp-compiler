//
// Created by Mathias Vatter on 19.07.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringVariableRef : public ASTLowering {
private:

public:
	explicit LoweringVariableRef(NodeProgram *program) : ASTLowering(program) {}

	inline NodeAST * visit(NodeVariableRef &node) override {

//		if(node.data_type != DataType::Const) {
//			return &node;
//		}

		if(node.is_array_constant()) {
			auto ref = node.declaration->to_reference();
			auto num_elements = std::make_unique<NodeNumElements>(
				std::move(ref),
				nullptr,
				node.tok
			);
			node.replace_with(std::move(num_elements))->lower(m_program)->accept(*this);
		}

		if(node.is_ndarray_constant()) {
			auto ref = node.declaration->to_reference();
			std::string prefix = ref->name+".SIZE_D";
			std::string number_str = node.name.substr(prefix.length());
			int dimension = std::stoi(number_str);
			auto num_elements = std::make_unique<NodeNumElements>(
				std::move(ref),
				std::make_unique<NodeInt>(dimension, node.tok),
				node.tok
			);
			node.replace_with(std::move(num_elements))->lower(m_program)->accept(*this);
		}

		return &node;
	}

};