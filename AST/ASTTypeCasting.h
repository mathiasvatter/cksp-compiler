//
// Created by Mathias Vatter on 31.10.23.
//

#pragma once

#include "ASTVisitor.h"

class ASTTypeCasting : public ASTVisitor {
public:
    explicit ASTTypeCasting(
            const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets,
            const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions);

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

    const std::unordered_map<std::string, std::unique_ptr<NodeUIControl>> &m_builtin_widgets;
//	const std::vector<std::unique_ptr<NodeUIControl>> &m_builtin_widgets;
	NodeUIControl* get_builtin_widget(const std::string &ui_control);

    const std::unordered_map<StringIntKey, std::unique_ptr<NodeFunctionHeader>, StringIntKeyHash> &m_builtin_functions;
    NodeFunctionHeader* get_builtin_function(const std::string &function, int params);

    std::unordered_map<int, ASTType> m_return_variables;

    std::unique_ptr<NodeAST> calculate_index_expression(const std::vector<std::unique_ptr<NodeAST>>& sizes, const std::vector<std::unique_ptr<NodeAST>>& indices, size_t dimension, const Token& tok);

};

