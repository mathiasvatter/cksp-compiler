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
    std::vector<Token> m_family_prefixes;

	void add_family_prefix(NodeDataStructure& ref) const {
		for (const auto &pref : m_family_prefixes) {
			ref.add_prefix(pref);
		}
    	auto prefixes = StringUtils::join_apply(
			m_family_prefixes,
			[](const Token& prefix) { return prefix.val; },
			"."
		);
    	ref.name = prefixes + "." + ref.name;
    }

public:
	explicit DesugarFamily(NodeProgram* program) : ASTDesugaring(program) {};

    NodeAST * visit(NodeVariable& node) override {
        add_family_prefix(node);
		return &node;
    }

	NodeAST * visit(NodePointer& node) override {
	    add_family_prefix(node);
    	return &node;
    }

	NodeAST * visit(NodeArray& node) override {
        if(node.size) node.size->accept(*this);
        add_family_prefix(node);
		return &node;
    }

	NodeAST * visit(NodeNDArray& node) override {
        if(node.sizes) node.sizes->accept(*this);
        add_family_prefix(node);
		return &node;
    }

	NodeAST * visit(NodeList& node) override {
        for(auto &b : node.body) {
            b->accept(*this);
        }
        add_family_prefix(node);
		return &node;
    }

    NodeAST * visit(NodeConst& node) override {
        add_family_prefix(node);
        node.constants->accept(*this);
		return &node;
    }

    NodeAST * visit(NodeFamily& node) override {
        const auto pref = node.prefix;
        m_family_prefixes.push_back(pref);
        node.members->accept(*this);
        m_family_prefixes.pop_back();
        return node.replace_with(std::move(node.members));
    };
};