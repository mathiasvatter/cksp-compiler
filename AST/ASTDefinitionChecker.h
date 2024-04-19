//
// Created by Mathias Vatter on 02.04.24.
//

#pragma once

#include "ASTVisitor.h"
#include "DefinitionProvider.h"

/// check all user variable definitions -> get pointers
/// check all function definitions -> get pointers
/// check all builtin var definitions -> get pointers
class ASTDefinitionChecker: public ASTVisitor {
public:
    explicit ASTDefinitionChecker(DefinitionProvider& def_provider);

    void visit(NodeProgram& node) override;
//    void visit(NodeSingleDeclareStatement& node) override;
//    void visit(NodeSingleAssignStatement& node) override;
    void visit(NodeUIControl& node) override;
//    void visit(NodeParamList& node) override;
    void visit(NodeVariable& node) override;
    void visit(NodeArray& node) override;
//    void visit(NodeInt& node) override;
//    void visit(NodeString& node) override;
//    void visit(NodeReal& node) override;
//    void visit(NodeBinaryExpr& node) override;
//    void visit(NodeUnaryExpr& node) override;
//    void visit(NodeFunctionCall& node) override;
//    void visit(NodeFunctionHeader& node) override;
    void visit(NodeStatementList& node) override;
//	void visit(NodeStatement& node) override;


    /// soft verification -> ignores missing variable declarations
    /// adds pointer to declaration and verifies that the variable has been declared
    void verify_array(NodeArray* arr, bool is_strict = false);
    /// adds pointer to declaration and verifies that the variable has been declared
    void verify_variable(NodeVariable* var, bool is_strict = false);

    /// handle two nodes that should have the same type
    ASTType match_types(NodeAST* probably_typed, NodeAST* not_typed);
    static bool type_mismatch(NodeAST* node1, NodeAST* node2);
private:

    DefinitionProvider& m_def_provider;

    /// multidimensional array method for getting the index at runtime
    std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok);

};





