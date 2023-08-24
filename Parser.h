//
// Created by Mathias Vatter on 24.08.23.
//

#pragma once

#include <Tokenizer.h>
#include <AST.h>

class Parser {

public:
	inline explicit Parser(std::vector<Token> tokens);

private:
	size_t m_pos;
	std::vector<Token> m_tokens;

	[[nodiscard]] std::optional<Token> peek(int ahead = 1) const;
	Token next_token(int tokens = 1);
};

