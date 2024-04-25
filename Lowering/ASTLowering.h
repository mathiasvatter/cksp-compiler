//
// Created by Mathias Vatter on 30.03.24.
//

#pragma once

#include "../AST/ASTNodes/AST.h"
#include "../AST/ASTVisitor/ASTVisitor.h"

/// Lowering of data structures to simpler data structures
/// e.g. Lists to arrays, multidimensional arrays to arrays
class ASTLowering: public ASTVisitor {
public:
    ASTLowering() = default;
    ~ASTLowering() = default;
};

