//
// Created by Mathias Vatter on 27.04.24.
//

#pragma once

#include "ASTDesugaring.h"

/**
 * Prepending family prefixes to all Datastructs in Family Block, namely:
 * - Variables, NDArrays, Arrays, Lists, Constblocks, UIControls
 */
class DesugarFamily final : public ASTDesugaring {
    std::stack<std::string> m_family_prefixes;
    std::string add_family_prefix(const std::string& name) {
        if(!m_family_prefixes.empty()) {
            return m_family_prefixes.top() + "." + name;
        }
        return name;
    }
public:
	explicit DesugarFamily(NodeProgram* program) : ASTDesugaring(program) {};

    NodeAST * visit(NodeVariable& node) override {
        node.name = add_family_prefix(node.name);
		return &node;
    }

	NodeAST * visit(NodeArray& node) override {
        if(node.size) node.size->accept(*this);
        node.name = add_family_prefix(node.name);
		return &node;
    }

	NodeAST * visit(NodeNDArray& node) override {
        if(node.sizes) node.sizes->accept(*this);
        node.name = add_family_prefix(node.name);
		return &node;
    }

	NodeAST * visit(NodeList& node) override {
        for(auto &b : node.body) {
            b->accept(*this);
        }
        node.name = add_family_prefix(node.name);
		return &node;
    }

    NodeAST * visit(NodeConst& node) override {
        node.name = add_family_prefix(node.name);
        node.constants->accept(*this);
		return &node;
    }

    NodeAST * visit(NodeFamily& node) override {
        std::string pref = node.prefix;
        if(!m_family_prefixes.empty()) pref = m_family_prefixes.top() + "." + node.prefix;
        m_family_prefixes.push(pref);
        node.members->accept(*this);
        m_family_prefixes.pop();
        return node.replace_with(std::move(node.members));
    };
};