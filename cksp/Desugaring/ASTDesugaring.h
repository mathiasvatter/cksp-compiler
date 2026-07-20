//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "../ASTVisitor/ASTVisitor.h"

/**
 * Desugaring is the process of rewriting syntactic sugar
 * constructs into more basic constructs.
 *
 */
class ASTDesugaring : public ASTVisitor {
public:
    explicit ASTDesugaring(NodeProgram* program) {m_program = program;};
    ~ASTDesugaring() =default;

	void set_program(NodeProgram* program) override {
		ASTVisitor::set_program(program);
	}

    std::unique_ptr<NodeAST> replacement_node;
};
