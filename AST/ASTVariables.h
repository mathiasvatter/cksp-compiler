//
// Created by Mathias Vatter on 08.12.23.
//

#pragma once


#include "ASTVisitor.h"

class ASTVariables : public ASTVisitor {
public:
    ASTVariables(const std::vector<std::unique_ptr<NodeFunctionHeader>> &m_builtin_functions,
                  const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables,
                  const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays);

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

    /// builtin engine variables
    const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables;
    NodeVariable* get_builtin_variable(NodeVariable* var);
    /// builtin engine arrays
    const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays;
    NodeArray* get_builtin_array(NodeArray* arr);

    /// declared variables
    std::vector<NodeVariable*> m_declared_variables;
    NodeVariable* get_declared_variable(NodeVariable* var);
    /// declared arrays
    std::vector<NodeArray*> m_declared_arrays;
    NodeArray* get_declared_array(const std::string& arr);
    /// declared ui_controls
    std::vector<NodeUIControl*> m_declared_controls;
    NodeUIControl* get_declared_control(NodeUIControl* arr);

    /// multidimensional array method for getting the index at runtime
    std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok);



};


