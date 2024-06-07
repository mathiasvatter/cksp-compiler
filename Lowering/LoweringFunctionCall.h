//
// Created by Mathias Vatter on 07.06.24.
//

#pragma once

#include "ASTLowering.h"

class LoweringFunctionCall : public ASTLowering {
public:
	explicit LoweringFunctionCall(DefinitionProvider *def_provider) : ASTLowering(def_provider) {}

	/// Determining if function parameter needs to be wrapped in get_ui_id because of ui control
	void visit(NodeFunctionCall &node) override {

	}
};