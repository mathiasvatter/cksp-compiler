//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreASTCombine.h"

void PreASTCombine::visit(PreNodeChunk& node) {
	for(auto & n : node.chunk) {
		n->accept(*this);
	}
}

void PreASTCombine::visit(PreNodeNumber &node) {
    m_tokens.push_back(std::move(node.value));
}

void PreASTCombine::visit(PreNodeInt &node) {
    m_tokens.push_back(std::move(node.value));
}

void PreASTCombine::visit(PreNodeKeyword &node) {
    m_tokens.push_back(std::move(node.value));
}

void PreASTCombine::visit(PreNodeOther &node) {
    m_tokens.push_back(std::move(node.other));
}

void PreASTCombine::visit(PreNodeProgram& node) {
	m_tokens.reserve(m_tokens.size() + node.program.size());
    for(auto & n : node.program) {
        n->accept(*this);
    }
}

void PreASTCombine::visit(PreNodeUnaryExpr& node) {
    m_tokens.push_back(std::move(node.op));
    node.operand->accept(*this);
}

void PreASTCombine::visit(PreNodeBinaryExpr& node) {
    node.left->accept(*this);
    m_tokens.push_back(std::move(node.op));
    node.right->accept(*this);
};

void PreASTCombine::visit(PreNodeIncrementer& node) {
    for(auto & b : node.body) {
        b->accept(*this);
    }
}

void PreASTCombine::visit(PreNodeList& node) {

	// create tokens for open parenthesis, closed parenthesis and comma
	auto reference_token = m_tokens.back();
	auto open_parenthesis = reference_token;
	open_parenthesis.type = token::OPEN_PARENTH;
	open_parenthesis.val = "(";
	auto closed_parenthesis = reference_token;
	closed_parenthesis.type = token::CLOSED_PARENTH;
	closed_parenthesis.val = ")";
	auto comma = reference_token;
	comma.type = token::COMMA;
	comma.val = ",";

	m_tokens.push_back(open_parenthesis);
	for(auto & param : node.params) {
		param->accept(*this);
		m_tokens.push_back(comma);
	}
	if(!node.params.empty()) m_tokens.pop_back();
	m_tokens.push_back(closed_parenthesis);
}

void PreASTCombine::visit(PreNodeMacroHeader& node) {
	node.name->accept(*this);
	if(node.has_parenth) {
		node.args->accept(*this);
	}
};
