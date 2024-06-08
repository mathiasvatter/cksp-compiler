//
// Created by Mathias Vatter on 02.12.23.
//

#pragma once

#include "ASTVisitor.h"

class ASTTypeChecking : public ASTVisitor {
public:
    void visit(NodeProgram& node) override;
    void visit(NodeCallback& node) override;
    void visit(NodeSingleDeclaration& node) override;
    void visit(NodeUIControl& node) override;

    void visit(NodeVariable& node) override;
    void visit(NodeVariableRef& node) override;
    void visit(NodeArray& node) override;
    void visit(NodeArrayRef& node) override;
	void visit(NodeBody& node) override;

private:
    NodeAST* m_current_node_replaced = nullptr;
    NodeCallback* m_init_callback = nullptr;

    int m_max_returns_in_current_callback = 0;
    int m_current_return_idx = 0;
    static int extract_last_number(const std::string& str, NodeAST* var);
    std::unique_ptr<NodeBody> declare_return_vars();

};


