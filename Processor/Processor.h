//
// Created by Mathias Vatter on 19.04.24.
//

#pragma once

#include "../Tokenizer/Tokenizer.h"
#include "../Tokenizer/Tokens.h"
#include "../AST/Types.h"

/// Base Class for all parsing related classes like:
/// - Parser
/// - Preprocessor
/// Implements basic functionality for token handling like peek() and consume()
class Processor {
public:
	virtual ~Processor() = default;
	explicit Processor(std::vector<Token> tokens);;
	explicit Processor() = default;

	virtual void process() {};
	/// returns the tokens vector by moving it
	std::vector<Token> get_token_vector();

protected:
	size_t m_pos = 0;
	std::vector<Token> m_tokens{};
	token m_curr_token_type = token::INVALID;
	std::string m_curr_token_value;
	Token m_curr_token;


	Token peek(const std::vector<Token>& tok, int ahead = 0);
	Token consume(const std::vector<Token>& tok);

	Token peek(int ahead = 0);
	Token consume();

	[[nodiscard]] const Token& get_tok(const std::vector<Token>& tok) const;
    [[nodiscard]] const Token& get_tok() const;

	void remove_tokens(std::vector<Token>& tok, size_t start, size_t end);

	/// \brief Parse a type annotation
	/// \param ty The optional identifier type before the variable name (e.g. ?|$|~x)
	Result<Type*> parse_type_annotation(Type* ty = nullptr);
	Result<Type*> parse_type();
	Result<Type*> _parse_function_type();
	Result<Type*> _parse_single_types();

	void _skip_linebreaks();

};

