//
// Created by Mathias Vatter on 27.04.24.
//

#pragma once

#include "ASTLowering.h"

/**
 * @brief This class is responsible for lowering the const statement.
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
	explicit LoweringConstStruct(NodeProgram* program) : ASTLowering(program) {}

    void visit(NodeVariable& node) override {
		if(node.ty == TypeRegistry::Unknown) node.ty = TypeRegistry::Integer;
        if(!m_const_prefixes.empty()) {
            node.name = m_const_prefixes.top() + "." + node.name;
        }
    };

    void visit(NodeSingleDeclaration& node) override {
        if (node.variable->get_node_type() != NodeType::Variable) {
            auto error = CompileError(ErrorType::SyntaxError,"", "", node.variable->tok);
			error.m_message = "Found incorrect <Constant Block> syntax. <Constant Blocks> can only contain <Variables>.";
			error.exit();
        }
        node.variable->accept(*this);
        // has no value -> create one
        if(!node.value) {
            node.value = std::make_unique<NodeBinaryExpr>(
                    token::ADD,
                    m_pre->clone(),
                    m_iter->clone(), node.tok
                    );
        } else {
            m_pre = node.value->clone();
            m_iter = std::make_unique<NodeInt>(0, node.tok);
        }
        node.variable->data_type = DataType::Const;
        node.set_child_parents();
    };

    void visit(NodeConstStatement& node) override {
        std::string pref = node.name;
        if(!m_const_prefixes.empty()) pref = m_const_prefixes.top() + "." + node.name;
        m_const_prefixes.push(pref);
        std::vector<std::unique_ptr<NodeAST>> const_indexes;
        m_iter = std::make_unique<NodeInt>(0, node.tok);
        m_pre = std::make_unique<NodeInt>(0, node.tok);
        for(int i = 0; i<node.constants->statements.size(); i++){
            node.constants->statements[i]->accept(*this);
            const_indexes.push_back(std::make_unique<NodeBinaryExpr>(
				token::ADD,
				m_pre->clone(),
				m_iter->clone(), node.tok)
			);
            m_iter = std::make_unique<NodeBinaryExpr>(
				token::ADD,
				m_iter->clone(),
				std::make_unique<NodeInt>(1, node.tok),
				node.tok
			);
        }
        auto node_array = std::make_unique<NodeArray>(
			std::nullopt,
			node.name,
            TypeRegistry::ArrayOfInt,
			std::make_unique<NodeInt>(node.constants->statements.size(), node.tok),
			node.tok
		);
        auto node_declare_statement = std::make_unique<NodeSingleDeclaration>(
			std::move(node_array),
			std::make_unique<NodeParamList>(std::move(const_indexes), node.tok),
			node.tok
		);
        auto array = std::make_unique<NodeStatement>(std::move(node_declare_statement), node.tok);
        node.constants->add_stmt(std::move(array));
        auto constant = make_declare_variable(node.name+".SIZE", node.constants->statements.size(), DataType::Const, node.constants.get());
        node.constants->add_stmt(std::move(constant));
        m_const_prefixes.pop();
    }
};