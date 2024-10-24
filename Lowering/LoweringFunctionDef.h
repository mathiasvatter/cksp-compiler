//
// Created by Mathias Vatter on 24.10.24.
//

#pragma once

#include "ASTLowering.h"

/// Lowering of Function Definitions with return statements by
/// - deleting all statements after return statements in the same block
/// - wrapping defs with return stmts (that are not at the end of the init block) in while loop
/// - placing exit_flag and continue statement after every return statement
/// -
class LoweringFunctionDef : public ASTLowering {
private:
public:
	explicit LoweringFunctionDef(NodeProgram *program) : ASTLowering(program) {}

	inline NodeAST* visit(NodeFunctionDefinition& node) override {
		return &node;
	}

};