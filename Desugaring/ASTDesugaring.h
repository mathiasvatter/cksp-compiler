//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

/**
 * Desugaring is the process of rewriting syntactic sugar
 * constructs into more basic constructs.
 *
 */
class ASTDesugaring : public ASTVisitor {
public:
    ASTDesugaring() =default;
    ~ASTDesugaring() =default;

    std::unique_ptr<NodeAST> replacement_node;
};
