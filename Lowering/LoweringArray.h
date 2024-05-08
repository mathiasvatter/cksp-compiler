//
// Created by Mathias Vatter on 08.05.24.
//

#pragma once

//
// Created by Mathias Vatter on 22.04.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * Determining the size of the array if possible
 * Throwing necessary errors if the array is not declared correctly
 * e.g. if size is not a constant or not able to be determined at compile time
 */
class LoweringArray : public ASTLowering {
private:
	bool size_is_constant = false;
public:
	explicit LoweringArray(DefinitionProvider* def_provider) : ASTLowering(def_provider) {}

	/// Determining array size at compile time
	void visit(NodeArray& node) override {
		// only check lowering if array is not a reference
		if (node.is_reference) return;

		auto error = CompileError(ErrorType::SyntaxError, "", "", node.tok);
		auto node_declaration = cast_node<NodeSingleDeclareStatement>(node.parent);
		if (!node_declaration) {
			error.m_message = "Array is not a reference even though it is not part of a declaration.";
			error.m_got = node.name;
			error.exit();
		}

		// infer size from assignee param list
		if (!node.size) {
			if (!node_declaration->assignee) {
				error.m_message =
					"Unable to infer array size. Size of Array has to be determined at compile time by providing an initializer list.";
				error.m_got = node.name;
				error.m_expected = "initializer list";
				error.exit();
			}
			if (auto param_list = cast_node<NodeParamList>(node_declaration->assignee.get())) {
				node.size = make_int((int32_t) param_list->params.size(), &node);
			}
		// array has size -> check if it is a constant
		} else {

		}

	}
};

