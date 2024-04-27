//
// Created by Mathias Vatter on 27.04.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * This class is responsible for lowering the const statement.
 * Example:
 * pre lowering:
 *  const fafa
 *		fufu
 *		ffifi
 *	end const
 * post lowering:
 * declare %fafa[2] := (0, 1)
 * declare const $fafa__SIZE := 2
 * declare const $fafa__fufu := 0
 * declare const $fafa__ffifi := 1
 */

class LoweringConstStruct : public ASTLowering {
private:
    std::stack<std::string> m_const_prefixes;
    std::unique_ptr<NodeAST> m_pre = nullptr;
    std::unique_ptr<NodeAST> m_iter = nullptr;

public:

    void visit(NodeVariable& node) override {
        if(!m_const_prefixes.empty() and is_to_be_declared(&node)) {
            node.name = m_const_prefixes.top() + "." + node.name;
        }
    };

    void visit(NodeSingleDeclareStatement& node) override {
        if (node.to_be_declared->get_node_type() != NodeType::Variable) {
            CompileError(ErrorType::SyntaxError,
                         "Found incorrect <const block> syntax. Only variables allowed in <const blocks>.", node.tok.line,"", "", node.tok.file).exit();
        }
        node.to_be_declared->accept(*this);
        // has no value -> create one
        if(!node.assignee) {
            node.assignee = std::make_unique<NodeBinaryExpr>("+", m_pre->clone(), m_iter->clone(), node.tok);
        } else {
            m_pre = node.assignee->clone();
            m_iter = make_int(0, node.parent);
        }
        node.to_be_declared->data_type = DataType::Const;
    };

    void visit(NodeConstStatement& node) override {
        std::string pref = node.name;
        if(!m_const_prefixes.empty()) pref = m_const_prefixes.top() + "." + node.name;
        m_const_prefixes.push(pref);
        std::vector<std::unique_ptr<NodeAST>> const_indexes;
        m_iter = make_int(0, node.parent);
        m_pre = make_int(0, node.parent);
        for(int i = 0; i<node.constants->statements.size(); i++){
            node.constants->statements[i]->accept(*this);
            const_indexes.push_back(std::make_unique<NodeBinaryExpr>("+", m_pre->clone(), m_iter->clone(), node.tok));
            m_iter = make_binary_expr(ASTType::Integer, "+", m_iter->clone(), make_int(1, node.parent), nullptr, node.tok);;
        }
        auto node_array = make_array(node.name, node.constants->statements.size(), node.tok, node.constants.get());
        auto node_declare_statement = std::make_unique<NodeSingleDeclareStatement>(std::move(node_array), nullptr, node.tok);
        node_declare_statement->assignee = std::unique_ptr<NodeParamList>(new NodeParamList(std::move(const_indexes), node.tok));
        auto array = std::make_unique<NodeStatement>(std::move(node_declare_statement), node.tok);
        array->update_parents(node.constants.get());
        node.constants->statements.push_back(std::move(array));
        auto constant = make_declare_variable(node.name+".SIZE", node.constants->statements.size(), DataType::Const, node.constants.get());
        node.constants->statements.push_back(std::move(constant));
        node.constants->update_parents(node.parent);
//        node.replace_with(std::move(node.constants));
        m_const_prefixes.pop();
    }
};