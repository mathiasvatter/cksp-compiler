//
// Created by Mathias Vatter on 30.03.24.
//

#pragma once

#include "../AST/ASTNodes/AST.h"
#include "../AST/ASTVisitor/ASTVisitor.h"
#include "../BuiltinsProcessing/DefinitionProvider.h"

/// Lowering of data structures to simpler data structures
/// e.g. Lists to arrays, multidimensional arrays to arrays
class ASTLowering: public ASTVisitor {
protected:
	DefinitionProvider* m_def_provider;
public:
    explicit ASTLowering(DefinitionProvider* def_provider) : m_def_provider(def_provider) {}
    ~ASTLowering() = default;

};

