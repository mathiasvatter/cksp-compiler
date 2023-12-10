//
// Created by Mathias Vatter on 31.10.23.
//

#pragma once

#include "ASTVisitor.h"

class ASTTypeCasting : public ASTVisitor {
public:
    ASTTypeCasting(const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables, const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays);

    void visit(NodeProgram& node) override;
    void visit(NodeSingleDeclareStatement& node) override;
    void visit(NodeUIControl& node) override;
    void visit(NodeParamList& node) override;
    void visit(NodeSingleAssignStatement& node) override;
    void visit(NodeVariable& node) override;
	void visit(NodeArray& node) override;
    void visit(NodeInt& node) override;
    void visit(NodeString& node) override;
    void visit(NodeReal& node) override;
    void visit(NodeBinaryExpr& node) override;
    void visit(NodeUnaryExpr& node) override;
    void visit(NodeFunctionCall& node) override;
    void visit(NodeFunctionHeader& node) override;
	void visit(NodeStatementList& node) override;
//	void visit(NodeStatement& node) override;

private:
    const std::vector<std::unique_ptr<NodeVariable>> &m_builtin_variables;
    const std::vector<std::unique_ptr<NodeArray>> &m_builtin_arrays;

    std::vector<NodeVariable*> m_declared_variables;
    NodeVariable* get_declared_variable(NodeVariable* var);
    std::vector<NodeArray*> m_declared_arrays;
    NodeArray* get_declared_array(const std::string& arr);

    std::vector<NodeUIControl*> m_declared_controls;
    NodeUIControl* get_declared_control(NodeUIControl* arr);

    NodeVariable* get_builtin_variable(NodeVariable* var);
    NodeArray* get_builtin_array(NodeArray* arr);
    std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok);

//    std::pair<ASTType, ASTType>

};

