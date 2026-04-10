//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Adds declaration stmts for dimension constants above all array declarations
 *  - has to happen after all ndarrays are lowered to arrays
 *  - entry point is NodeSingleDeclaration
 * > has to happen after variable reuse because of the renaming of local arrays
 * Checks if declaration is array which has num_elements member -> adds declaration for num_elements array
 * pre-lowering:
 *  declare arr[10] := (....)
 * post-lowering
 *  declare arr[10] := (.....)
 *  declare const arr::num_elements0 := 10
 */
class ArrayDimensionConstants final : public ASTLowering {

public:
	explicit ArrayDimensionConstants(NodeProgram *program) : ASTLowering(program) {}

	NodeAST* visit(NodeSingleDeclaration &node) override {
		if(auto node_array = node.variable->cast<NodeArray>()) {
			if(node_array->num_elements) {
				auto node_block = std::make_unique<NodeBlock>(node.tok);
				for (int i = 0; i<node_array->num_elements->params.size(); i++) {
					auto& el = node_array->num_elements->params[i];
					auto node_num_elements = std::make_shared<NodeVariable>(
						std::optional<Token>(),
						node_array->name + OBJ_DELIMITER+"num_elements"+std::to_string(i),
						TypeRegistry::Integer,
						node.tok,
						DataType::Const
					);
					auto node_num_elements_decl = std::make_unique<NodeSingleDeclaration>(
						node_num_elements,
						std::move(el),
						node.tok
					);
					node_block->add_as_stmt(std::move(node_num_elements_decl));

				}
				node_array->num_elements = std::make_unique<NodeParamList>(node.tok);

				// wip fix: if array declaration is in global scope -> add to end of init callback
				//  >> WHY this? at this point no global scope exists anymore....
				// if (node.get_parent_block() == m_program->global_declarations.get()) {
				// 	m_program->init_callback->statements->append_body(std::move(node_block));
				// 	return &node;
				// }

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