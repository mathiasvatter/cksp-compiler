//
// Created by Mathias Vatter on 27.04.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * Prepending family prefixes to all Datastructs in Family Block
 */
class LoweringFamilyStruct : public ASTLowering {
private:
    std::stack <std::string> m_family_prefixes;

public:

    void visit(NodeSingleDeclareStatement& node) override {
        if(!m_family_prefixes.empty()) {
            // Ui control data structures need to go one level deeper
            if(node.to_be_declared->get_node_type() != NodeType::UIControl) {
                node.to_be_declared->name = m_family_prefixes.top() + "." + node.to_be_declared->name;
            } else {
                node.to_be_declared ->accept(*this);
            }
        }
    };

    void visit(NodeConstStatement& node) override {
        if(!m_family_prefixes.empty()) {
            node.name = m_family_prefixes.top() + "." + node.name;
        }
    };

    void visit(NodeUIControl& node) override {
        if(!m_family_prefixes.empty()) {
            node.control_var->name = m_family_prefixes.top() + "." + node.control_var->name;
        }
    };

    void visit(NodeFamilyStatement& node) {
        std::string pref = node.prefix;
        if(!m_family_prefixes.empty()) pref = m_family_prefixes.top() + "." + node.prefix;
        m_family_prefixes.push(pref);
        node.members->accept(*this);
//        node.replace_with(std::move(node.members));
        m_family_prefixes.pop();
    };
};