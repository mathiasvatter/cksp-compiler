//
// Created by Mathias Vatter on 21.01.24.
//

#pragma once

#include "ASTVisitor.h"
#include "../../Desugaring/ASTDesugaring.h"

class ASTDesugarStructs: public ASTVisitor {
    void visit(NodeProgram& node) override;

    /// desugar into single declare statements
    void visit(NodeDeclareStatement& node) override;
    /// desugar into single assign statements
    void visit(NodeAssignStatement& node) override;

    /// desugar for each loops to for loops
	void visit(NodeForEachStatement& node) override;
    /// alter for loops to while loops
    void visit(NodeForStatement& node) override;

    void visit(NodeBody& node) override;

private:
    NodeProgram* m_program = nullptr;

	std::vector<std::unordered_map<std::string, std::unique_ptr<NodeAST>>> m_key_value_scope_stack;
	std::unique_ptr<NodeAST> get_key_value_substitute(const std::string& name);
};

