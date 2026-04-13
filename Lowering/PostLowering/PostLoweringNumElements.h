//
// Created by Mathias Vatter on 31.10.24.
//

#pragma once

#include "../ASTLowering.h"

/**
 * Second Lowering Phase of num_elements
 * has to happen after function inlining because we need the actual name of the array we are getting
 * the size of, so the first arg needs to have been substituted
 * if num_elements member in array declaration is given -> create declaration for num_elements array
 * 	declare ndarray::num_elements[3] := (3*3, 3, 3)
 * if no dimension is given -> was array -> call to num_elements
 * >> special case: a local function array has a size that depends on a substituted function parameter.
 * -> declare urmom[num_elements(arr)]
 * it will have been put at the end of global declaration by VariableReuse even though arr
 * might have been declared elsewhere. In this case we will just get the size of the declaration of arr
 * and replace NodeNumElements with it. This works also with ndarrays (that are already lowered here)
 */
class PostLoweringNumElements final : public ASTLowering {

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
				node.dimension->do_constant_folding();
				auto dim_int = node.dimension->cast<NodeInt>();
				if (!dim_int) {
					auto error = CompileError(ErrorType::SyntaxError, "", "", node.dimension->tok);
					error.m_message = "The dimension of <num_elements> could not be folded into an <integer>.";
					error.exit();
				}

				// auto node_block = std::make_unique<NodeBlock>(node_array->tok);

				auto num_elements_call = std::make_unique<NodeVariableRef>(
					node_array->name + OBJ_DELIMITER+"num_elements" + std::to_string(dim_int->value),
					TypeRegistry::Integer,
					node.tok
				);
				return node.replace_with(std::move(num_elements_call));
			} else if (node.array->is_local) {
				// checks if the array node is local which means we need to do substitute the this size node
				return node.replace_with(decl->get_size());
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