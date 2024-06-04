//
// Created by Mathias Vatter on 19.04.24.
//

#include "Processor.h"
#include "../AST/TypeRegistry.h"

Processor::Processor(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {
	if(!m_tokens.empty())
		m_curr_token = m_tokens.at(0).type;
}

Token Processor::peek(const std::vector<Token> &tok, int ahead) {
	if (tok.size() < m_pos+ahead) {
		auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).exit();
	}
	m_curr_token = tok.at(m_pos).type;
	m_curr_token_value = tok.at(m_pos).val;
	return tok.at(m_pos+ahead);
}

Token Processor::peek(int ahead) {
	return peek(m_tokens, ahead);
}

Token Processor::consume(const std::vector<Token> &tok) {
	if (m_pos >= tok.size()) {
		auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).exit();
	}
	m_curr_token = tok.at(m_pos + 1).type;
	m_curr_token_value = tok.at(m_pos+1).val;
	return tok.at(m_pos++);
}

Token Processor::consume() {
	return consume(m_tokens);
}

const Token &Processor::get_tok(const std::vector<Token> &tok) const {
	return tok.at(m_pos);
}

const Token &Processor::get_tok() const {
    return m_tokens.at(m_pos);
}

std::vector<Token> Processor::get_token_vector() {
	return std::move(m_tokens);
}

void Processor::remove_tokens(std::vector<Token> &tok, size_t start, size_t end) {
	if (start < end && end <= tok.size()) {
		tok.erase(tok.begin() + start, tok.begin() + end);
		// Adjust m_pos if necessary
		if (m_pos > start)
			m_pos -= (end - start);
	} else {
		auto err_msg = "Attempted to remove a token range out of bounds.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "unknown", tok.at(m_pos).val, tok.at(m_pos).file).print();
		exit(EXIT_FAILURE);
	}
}

Result<Type*> Processor::parse_type_annotation(Type* ty) {
	Type* type = TypeRegistry::Unknown;
	if(peek().type == token::TYPE) {
		consume(); // consume semicolon
		auto error = CompileError(ErrorType::ParseError,"", "", get_tok());
		if(peek().type != token::KEYWORD) {
			error.m_message = "Found incorrect Type annotation syntax.";
			return Result<Type*>(error);
		}
		auto type_token = consume(m_tokens);
		int dimensions = 0;
		while (peek().type == token::OPEN_BRACKET) {
			consume(); // consume the open bracket [
			if (peek().type != token::CLOSED_BRACKET) {
				error.m_message = "Expected closed bracket in Type annotation syntax.";
				error.m_expected = type_token.val + "[]";
				return Result<Type*>(error);
			}
			consume(); // consume the closed bracket
			dimensions++;
		}
		type = TypeRegistry::get_type_from_annotation(type_token.val);
		if(!type) {
			error.m_message = "Unknown Type annotation.";
			error.m_expected = "valid type annotation";
			return Result<Type*>(error);
		}
		// if composite type
		if(dimensions>0) {
			type = TypeRegistry::add_composite_type(CompoundKind::Array, type, dimensions);
		}
		// if existing type is already not Unknown then there was an identifier
		if(ty and ty != TypeRegistry::Unknown and ty != type) {
			error.m_message = "Found identifier combined with differing type annotation. Identifier type must match annotation type.";
			error.m_expected = type->to_string();
			error.m_got = ty->to_string();
			return Result<Type*>(error);
		}
	// if no type annotation was provided but an identifier was
	} else {
		if(ty) type = ty;
	}
	return Result<Type*>(type);
}