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
	NodeProgram* m_program = nullptr;
public:
	NodeAST* lowered_node = nullptr;

    explicit ASTLowering(NodeProgram* program) : m_program(program), m_def_provider(program->def_provider) {}
    ~ASTLowering() = default;

};

