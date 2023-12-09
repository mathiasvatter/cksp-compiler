//
// Created by Mathias Vatter on 08.12.23.
//

#pragma once


#include "ASTVisitor.h"

class ASTVariables : public ASTVisitor {
public:
    explicit ASTVariables(const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions);

    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeFunctionCall& node) override;

    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeSingleAssignStatement& node) override;

    void visit(NodeStatementList& node) override;

    void visit(NodeArray& node) override;
    void visit(NodeVariable& node) override;

private:
    NodeCallback* m_init_callback;

    const std::vector<std::unique_ptr<NodeFunctionHeader>>& m_builtin_functions;
    NodeFunctionHeader* get_builtin_function(const std::string &function);

//    const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables;
//    const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays;

//    std::vector<NodeVariable*> m_declared_variables;
//    NodeVariable* get_declared_variable(NodeVariable* var);
//    std::vector<NodeArray*> m_declared_arrays;
//    NodeArray* get_declared_array(NodeArray* arr);

    /// multidimensional arrays
    std::unique_ptr<NodeAST> create_right_nested_binary_expr(const std::vector<std::unique_ptr<NodeAST>>& nodes, size_t index, const std::string& op, const Token& tok);


    std::vector<std::unique_ptr<NodeStatement>> add_read_functions(NodeAST* var, NodeAST* parent);

};


