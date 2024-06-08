//
// Created by Mathias Vatter on 27.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Prepending family prefixes to all Datastructs in Family Block, namely:
 * - Variables, NDArrays, Arrays, Lists, Constblocks, UIControls
 */
class DesugaringFamilyStruct : public ASTDesugaring {
private:
    std::stack<std::string> m_family_prefixes;
    std::string add_family_prefix(const std::string& name) {
        if(!m_family_prefixes.empty()) {
            return m_family_prefixes.top() + "." + name;
        }
        return name;
    }
public:
//    void visit(NodeSingleDeclaration& node) override {
//        if(!m_family_prefixes.empty()) {
//            // Ui control data structures need to go one level deeper
//            if(node.variable->get_node_type() != NodeType::UIControl) {
//                node.variable->name = m_family_prefixes.top() + "." + node.variable->name;
//            } else {
//                node.variable ->accept(*this);
//            }
//        }
//    };

    void visit(NodeVariable& node) override {
        node.name = add_family_prefix(node.name);
    };
//    void visit(NodeVariableRef& node) override {
//        node.name = add_family_prefix(node.name);
//    };
    void visit(NodeArray& node) override {
        if(node.size) node.size->accept(*this);
        node.name = add_family_prefix(node.name);
    };
//    void visit(NodeArrayRef& node) override {
//        if(node.index) node.index->accept(*this);
//        node.name = add_family_prefix(node.name);
//    };
    void visit(NodeNDArray& node) override {
        if(node.sizes) node.sizes->accept(*this);
        node.name = add_family_prefix(node.name);
    };
//    void visit(NodeNDArrayRef& node) override {
//        if(node.indexes) node.indexes->accept(*this);
//        node.name = add_family_prefix(node.name);
//    };
    void visit(NodeListStruct& node) override {
        for(auto &b : node.body) {
            b->accept(*this);
        }
        node.name = add_family_prefix(node.name);
    };
//    void visit(NodeListStructRef& node) override {
//        node.name = add_family_prefix(node.name);
//        if(node.indexes) node.indexes->accept(*this);
//    };

    void visit(NodeConstStatement& node) override {
        node.name = add_family_prefix(node.name);
        node.constants->accept(*this);
    };
//
//    void visit(NodeUIControl& node) override {
//        if(!m_family_prefixes.empty()) {
//            node.control_var->name = m_family_prefixes.top() + "." + node.control_var->name;
//        }
//    };

    void visit(NodeFamily& node) override {
        std::string pref = node.prefix;
        if(!m_family_prefixes.empty()) pref = m_family_prefixes.top() + "." + node.prefix;
        m_family_prefixes.push(pref);
        node.members->accept(*this);
//        node.replace_with(std::move(node.members));
        m_family_prefixes.pop();
    };
};