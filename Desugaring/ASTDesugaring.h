//
// Created by Mathias Vatter on 28.04.24.
//

#pragma once

#include "../AST/ASTVisitor/ASTVisitor.h"

class ASTDesugaring : public ASTVisitor {
public:
    ASTDesugaring() =default;
    ~ASTDesugaring() =default;
//    std::unique_ptr<NodeAST> get_replacement_node() {
//        return std::move(replacement_node);
//    }
    std::unique_ptr<NodeAST> replacement_node;
};
