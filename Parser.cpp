//
// Created by Mathias Vatter on 24.08.23.
//

#include "Parser.h"

#include <utility>

Parser::Parser(std::vector<Token> tokens): m_tokens(std::move(tokens)) {
	m_pos = 0;
}

std::optional<Token> Parser::peek(int ahead) const {
	if (m_tokens.size() < m_pos+ahead)
		return {};
	return m_tokens.at(m_pos+ahead);

}

Token Parser::next_token(int tokens) {
	return m_tokens.at(m_pos++);
}

