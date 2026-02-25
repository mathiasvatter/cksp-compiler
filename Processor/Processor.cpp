//
// Created by Mathias Vatter on 19.04.24.
//

#include "Processor.h"
#include "../AST/TypeRegistry.h"

Processor::Processor(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {
	if(!m_tokens.empty())
		m_curr_token_type = m_tokens.at(0).type;
}

Token Processor::peek(const std::vector<Token> &tok, const int ahead) {
	if (tok.size() < m_pos+ahead) {
		auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).exit();
	}
	m_curr_token = tok.at(m_pos);
	m_curr_token_type = m_curr_token.type;
	m_curr_token_value = m_curr_token.val;
	return tok.at(m_pos+ahead);
}

Token Processor::peek(const int ahead) {
	return peek(m_tokens, ahead);
}

Token Processor::consume(const std::vector<Token> &tok) {
	if (m_pos >= tok.size()) {
		const auto err_msg = "Reached the end of the tokens. Wrong Syntax discovered.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "end token", tok.at(m_pos).val, tok.at(m_pos).file).exit();
	}
	if (m_pos + 1 < tok.size()) {
		m_curr_token = tok.at(m_pos + 1);
		m_curr_token_type = m_curr_token.type;
		m_curr_token_value = m_curr_token.val;
	}
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

void Processor::remove_tokens(std::vector<Token> &tok, const size_t start, const size_t end) {
	if (start < end && end <= tok.size()) {
		tok.erase(start + tok.begin(), tok.begin() + end);
		// Adjust m_pos if necessary
		if (m_pos > start)
			m_pos -= (end - start);
	} else {
		const auto err_msg = "Attempted to remove a token range out of bounds.";
		CompileError(ErrorType::PreprocessorError, err_msg, tok.at(m_pos).line, "unknown", tok.at(m_pos).val, tok.at(m_pos).file).print();
		exit(EXIT_FAILURE);
	}
}

Result<Type*> Processor::parse_type_annotation(Type* ty) {
	Type* type = TypeRegistry::Unknown;
	if(peek().type == token::TYPE) {
		consume(); // consume semicolon
		auto parsed_type = parse_type();
		if(parsed_type.is_error()) return Result<Type*>(parsed_type.get_error());
		type = parsed_type.unwrap();
		// if existing type is already not Unknown then there was an identifier
		if (ty and ty != TypeRegistry::Unknown and ty != type) {
			auto error = CompileError(ErrorType::ParseError,"", "", get_tok());
			error.m_message = "Found identifier combined with differing type annotation. Identifier type must match annotation type.";
			error.m_expected = type->to_string();
			error.m_got = ty->to_string();
			return Result<Type *>(error);
		}
	// if no type annotation was provided but an identifier was
	} else {
		if(ty) type = ty;
	}
	return Result<Type*>(type);
}

Result<Type*> Processor::parse_type() {
	Type* type = TypeRegistry::Unknown;
	auto error = CompileError(ErrorType::ParseError,"", "", get_tok());
	if(peek().type != token::KEYWORD and peek().type != token::OPEN_PARENTH) {
		error.m_message = "Found incorrect Type annotation syntax.";
		return Result<Type*>(error);
	}

	if(peek().type == token::OPEN_PARENTH) {
		auto func_type = _parse_function_type();
		if(func_type.is_error()) return Result<Type*>(func_type.get_error());
		type = func_type.unwrap();
	} else if(peek().type == token::KEYWORD) {
		auto single_type = _parse_single_types();
		if(single_type.is_error()) return Result<Type*>(single_type.get_error());
		type = single_type.unwrap();
	}
	return Result<Type*>(type);
}


Result<Type*> Processor::_parse_single_types() {
	auto type_token = consume(m_tokens);
	Type* type = nullptr;
	int dimensions = 0;
	auto error = CompileError(ErrorType::ParseError,"", "", get_tok());
	while (peek().type == token::OPEN_BRACKET) {
		consume(); // consume the open bracket [
		if (peek().type != token::CLOSED_BRACKET) {
			error.m_message = "Expected closed bracket in Type annotation syntax.";
			error.m_expected = type_token.val + "[]";
			return Result<Type *>(error);
		}
		consume(); // consume the closed bracket
		dimensions++;
	}
	type = TypeRegistry::get_type_from_annotation(type_token.val);
	// check if type object if type still unknown
	if (type == TypeRegistry::Unknown) {
		type = TypeRegistry::add_object_type(type_token.val);
	}
	if (!type) {
		error.m_message = "Unknown Type annotation.";
		error.m_expected = "valid type annotation";
		return Result<Type *>(error);
	}
	// if composite type
	if (dimensions > 0) {
		type = TypeRegistry::add_composite_type(CompoundKind::Array, type, dimensions);
	}
	return Result<Type*>(type);
}

Result<Type*> Processor::_parse_function_type() {
	std::vector<Type*> params;
	// consume the open parenthesis
	consume();
	while(peek().type != token::CLOSED_PARENTH) {
		auto type = parse_type();
		if(type.is_error()) return Result<Type*>(type.get_error());
		params.push_back(type.unwrap());
		if(peek().type == token::COMMA) {
			consume();
		}
	}
	consume(); // consume the closed parenthesis
	if(peek().type != token::TYPE) {
		auto error = CompileError(ErrorType::ParseError,"", "", get_tok());
		error.m_message = "Found incorrect Function type syntax. Expected return type annotation.";
		error.m_got = peek().val;
		error.m_expected = ":";
		return Result<Type*>(error);
	}
	consume(); // consume the colon
	auto return_type = parse_type();
	if(return_type.is_error()) return Result<Type*>(return_type.get_error());
	return Result<Type*>(TypeRegistry::add_function_type(params, return_type.unwrap()));
}

void Processor::_skip_linebreaks() {
	while(peek().type == token::LINEBRK){
		consume();
	}
}
