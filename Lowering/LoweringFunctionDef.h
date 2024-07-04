//
// Created by Mathias Vatter on 04.07.24.
//

#pragma once

#include "ASTLowering.h"

/// Lowers to retun statements when necessary
class LoweringFunctionDef : public ASTLowering {
private:

public:
	explicit LoweringFunctionDef(NodeProgram *program) : ASTLowering(program) {}



};