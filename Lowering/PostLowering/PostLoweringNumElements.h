//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Second Lowering Phase of num_elements
 * has to happen after function inlining
 * if num_elements member in array declaration is given -> create declaration for num_elements array
 * 	declare ndarray::num_elements[3] := (3*3, 3, 3)
 * if no dimension is given -> was array -> call to num_elements
 */
class PostLoweringNumElements : public ASTLowering {

public:
	explicit PostLoweringNumElements(NodeProgram *program) : ASTLowering(program) {}

	NodeAST * visit(NodeNumElements &node) override {

		auto decl = node.array->get_declaration();
		if(!decl) {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "Declaration of array in <num_elements> node was not set.";
			error.exit();
		}

		// check if num_elements member is set ->
		// if yes -> create declaration for num_elements array
		// if not -> call to num_elements
		if(auto node_array = decl->cast<NodeArray>()) {
			// in case nd array was written in raw array form -> declaration has num_elements but no dimension
			if(node_array->num_elements and node.dimension) {
				auto node_block = std::make_unique<NodeBlock>(node_array->tok);

				auto num_elements_call = std::make_unique<NodeArrayRef>(
					node_array->name + OBJ_DELIMITER+"num_elements",
					std::move(node.dimension),
					node.tok
				);
				num_elements_call->ty = TypeRegistry::Integer;
				return node.replace_with(std::move(num_elements_call));

			} else {
				// use builtin num_elements function
				return node.replace_with(DefinitionProvider::num_elements(std::move(node.array)));
			}
		} else {
			auto error = CompileError(ErrorType::InternalError, "", "", node.tok);
			error.m_message = "<reference> node in <num_elements> has somehow a declaration that is not an <Array>.";
			error.exit();
		}
		return &node;
	}

};