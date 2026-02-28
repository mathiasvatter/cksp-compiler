//
// Created by Mathias Vatter on 18.11.23.
//

#include "PreASTCombine.h"

PreNodeAST *PreASTCombine::visit(PreNodeChunk &node) {
	visit_all(node.chunk, *this);
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeNumber &node) {
    m_tokens.push_back(std::move(node.tok));
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeInt &node) {
    m_tokens.push_back(std::move(node.tok));
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeKeyword &node) {
    m_tokens.push_back(std::move(node.tok));
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeOther &node) {
    m_tokens.push_back(std::move(node.tok));
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeProgram &node) {
	m_tokens.reserve(m_tokens.size() + node.program->num_chunks());
	node.program->accept(*this);
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeUnaryExpr &node) {
    m_tokens.push_back(std::move(node.tok));
    node.operand->accept(*this);
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeBinaryExpr &node) {
    node.left->accept(*this);
    m_tokens.push_back(std::move(node.tok));
    node.right->accept(*this);
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeIncrementer &node) {
	visit_all(node.body, *this);
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeList &node) {

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
	return &node;
}

PreNodeAST *PreASTCombine::visit(PreNodeMacroHeader &node) {
	node.name->accept(*this);
	if(node.has_parenth) {
		node.args->accept(*this);
	}
	return &node;
}

PreNodeAST * PreASTCombine::visit(PreNodeFunctionHeader &node) {
	node.name->accept(*this);
	if(node.has_parenth) {
		node.args->accept(*this);
	}
	return &node;
}

PreNodeAST * PreASTCombine::visit(PreNodeDefineHeader &node) {
	node.name->accept(*this);
	if(node.has_parenth) {
		node.args->accept(*this);
	}
	return &node;
}

void PreASTCombine::debug_print_tokens(const std::string &path) const {
	std::ostringstream os;
	for (const auto& tok : m_tokens) {
		os << tok.val;
	}

	std::ofstream out_file(path);
	if (out_file) {
		out_file << os.str() << " ";
	} else {
		// Fehlerbehandlung, falls die Datei nicht geöffnet werden kann
		std::cerr << "Fehler beim Öffnen der Datei: " << path << std::endl;
	}
	os.str("");
}
