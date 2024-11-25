//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Second Lowering Phase of SingleDeclaration
 * has to happen after function inlining
 * Checks if declaration is array which has num_elements member -> adds declaration for num_elements array
 */
class PostLoweringSingleDeclaration : public ASTLowering {
private:

public:
	explicit PostLoweringSingleDeclaration(NodeProgram *program) : ASTLowering(program) {}

	NodeAST* visit(NodeSingleDeclaration &node) override {
		if(auto node_array = node.variable->cast<NodeArray>()) {
			if(node_array->num_elements) {
				auto node_block = std::make_unique<NodeBlock>(node.tok);
				auto node_num_elements = std::make_shared<NodeArray>(
					std::optional<Token>(),
					node_array->name + OBJ_DELIMITER+"num_elements",
					TypeRegistry::ArrayOfInt,
					std::make_unique<NodeInt>(node_array->num_elements->size(), node.tok),
					node.tok
				);
				auto node_num_elements_decl = std::make_unique<NodeSingleDeclaration>(
					node_num_elements,
					node_array->num_elements->to_initializer_list(),
					node.tok
				);
				node_num_elements_decl->variable->data_type = DataType::Const;
				node_block->add_as_stmt(std::move(node_num_elements_decl));

				auto node_array_decl = std::make_unique<NodeSingleDeclaration>(
					std::move(node.variable),
					std::move(node.value),
					node.tok
				);
				node_block->add_as_stmt(std::move(node_array_decl));
				return node.replace_with(std::move(node_block));
			}
		}
		return &node;
	}

};