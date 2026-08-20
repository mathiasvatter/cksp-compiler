//
// Created by Mathias Vatter on 19.04.24.
//

#include "Processor.h"
#include "../ASTNodes/ASTDataStructures.h"
#include "../Types/TypeRegistry.h"

Processor::Processor(std::vector<Token> tokens) : m_tokens(std::move(tokens)) {
	if(!m_tokens.empty()) {
		m_curr_token_type = m_tokens.at(0).type;
		m_curr_token_value = m_tokens.at(0).val;
		m_curr_token = m_tokens.at(0);
	}
}

/// The tokens ran out mid-construct. Which construct that is, this layer cannot know - it
/// only hands out tokens - but "the file ends here" is the one thing worth saying, and it
/// is what an editor shows on every half-written block.
std::string Processor::unterminated_construct_message() {
	return "The file ends while a construct is still open. Every <on>, <function>, <macro>,"
		" <if>, <for>, <while>, <select> and <struct> needs its closing <end ...>.";
}

const Token &Processor::peek(const std::vector<Token> &tok, const int ahead) {
	const auto idx = static_cast<long long>(m_pos) + ahead;
	if (idx < 0 || idx >= static_cast<long long>(tok.size())) {
		Diagnostic(ErrorType::PreprocessorError, unterminated_construct_message(),
			"a closing <end ...>", m_curr_token).exit();
	}
	m_curr_token = tok[m_pos];
	m_curr_token_type = m_curr_token.type;
	m_curr_token_value = m_curr_token.val;
	return tok[static_cast<size_t>(idx)];
}

const Token &Processor::peek(const int ahead) {
	return peek(m_tokens, ahead);
}

const Token &Processor::consume(const std::vector<Token> &tok) {
	if (m_pos >= tok.size()) {
		Diagnostic(ErrorType::PreprocessorError, unterminated_construct_message(),
			"a closing <end ...>", m_curr_token).exit();
	}
	if (m_pos + 1 < tok.size()) {
		m_curr_token = tok[m_pos + 1];
		m_curr_token_type = m_curr_token.type;
		m_curr_token_value = m_curr_token.val;
	}
	return tok[m_pos++];
}

const Token &Processor::consume() {
	return consume(m_tokens);
}

token Processor::peek_type(const int ahead) const {
	const auto idx = static_cast<long long>(m_pos) + ahead;
	if (idx < 0 || idx >= static_cast<long long>(m_tokens.size())) {
		Diagnostic(ErrorType::PreprocessorError, unterminated_construct_message(),
			"a closing <end ...>", m_curr_token).exit();
	}
	return m_tokens[static_cast<size_t>(idx)].type;
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
		Diagnostic(ErrorType::PreprocessorError, err_msg, "unknown", m_curr_token).exit();
	}
}

Result<Type*> Processor::parse_type_annotation(Type* ty, TypeReferences* references) {
	Type* type = TypeRegistry::Unknown;
	if(peek().type == token::TYPE) {
		consume(); // consume semicolon
		auto parsed_type = parse_type(references);
		if(parsed_type.is_error()) return Result<Type*>(parsed_type.get_error());
		type = parsed_type.unwrap();
		// if existing type is already not Unknown then there was an identifier
		if (ty and ty != TypeRegistry::Unknown and ty != type) {
			auto error = Diagnostic(ErrorType::ParseError,"", "", get_tok());
			error.message = "Found identifier combined with differing type annotation. Identifier type must match annotation type.";
			error.expected = type->to_string();
			error.actual = ty->to_string();
			return Result<Type *>(error);
		}
	// if no type annotation was provided but an identifier was
	} else {
		if(ty) type = ty;
	}
	return Result<Type*>(type);
}

Result<Type*> Processor::parse_type(TypeReferences* references) {
	Type* type = TypeRegistry::Unknown;
	auto error = Diagnostic(ErrorType::ParseError,"", "", get_tok());
	if(peek().type != token::KEYWORD and peek().type != token::OPEN_PARENTH) {
		error.message = "Found incorrect Type annotation syntax.";
		return Result<Type*>(error);
	}

	if(peek().type == token::OPEN_PARENTH) {
		auto func_type = _parse_function_type(references);
		if(func_type.is_error()) return Result<Type*>(func_type.get_error());
		type = func_type.unwrap();
	} else if(peek().type == token::KEYWORD) {
		auto single_type = _parse_single_types(references);
		if(single_type.is_error()) return Result<Type*>(single_type.get_error());
		type = single_type.unwrap();
	}
	return Result<Type*>(type);
}


Result<Type*> Processor::_parse_single_types(TypeReferences* references, const bool allow_parameterized) {
	auto type_token = consume(m_tokens);
	Type* type = nullptr;
	std::vector<Type*> type_arguments;
	int dimensions = 0;
	auto error = Diagnostic(ErrorType::ParseError,"", "", get_tok());
	if (peek().type == token::LESS_THAN) {
		if (!allow_parameterized) {
			error.set_token(peek());
			error.message = "Parameterized type <" + type_token.val
				+ "> cannot be used as a type argument yet.";
			error.expected = "a non-parameterized type such as <int> or <Note>";
			return Result<Type*>(error);
		}
		auto arguments = _parse_type_arguments(type_token, references);
		if (arguments.is_error()) return Result<Type*>(arguments.get_error());
		type_arguments = arguments.unwrap();
	}
	while (peek().type == token::OPEN_BRACKET) {
		consume(); // consume the open bracket [
		if (peek().type != token::CLOSED_BRACKET) {
			error.message = "Expected closed bracket in Type annotation syntax.";
			error.expected = type_token.val + "[]";
			return Result<Type *>(error);
		}
		consume(); // consume the closed bracket
		dimensions++;
	}

	type = TypeRegistry::get_type_from_annotation(type_token.val);
	if (!type_arguments.empty() && type != TypeRegistry::Unknown) {
		error.set_token(type_token);
		error.message = "Built-in type <" + type_token.val + "> cannot have type arguments.";
		error.expected = "a struct type such as <List<int>>";
		return Result<Type*>(error);
	}
	// check if type object if type still unknown
	if (type == TypeRegistry::Unknown) {
		if (const auto* strct = current_struct()) {
			auto it = strct->type_parameter_table.find(type_token.val);
			if (it != strct->type_parameter_table.end()) {
				type = it->second.get();
			}
		}
		// A name that is not a parameter is still an ordinary struct type, even while
		// another struct is being parsed.
		if (type == TypeRegistry::Unknown) {
			type = TypeRegistry::add_object_type(type_token.val);
		}
	}
	if (!type) {
		error.message = "Unknown Type annotation.";
		error.expected = "valid type annotation";
		return Result<Type *>(error);
	}
	// Retain every written type name independently from the interned semantic Type.
	// For composite annotations such as Foo[][] this intentionally records Foo,
	// while the returned semantic type is wrapped below.
	if (references) references->push_back({type, type_token});
	if (!type_arguments.empty()) {
		type = TypeRegistry::add_object_type(type_token.val, type_arguments);
	}
	// if composite type
	if (dimensions > 0) {
		type = TypeRegistry::add_composite_type(CompoundKind::Array, type, dimensions);
	}
	return Result<Type*>(type);
}

Result<std::vector<Type*>> Processor::_parse_type_arguments(
		const Token& type_token, TypeReferences* references) {
	std::vector<Type*> arguments;
	const auto reached_construct_end = [this]() {
		return peek().type == token::END_TOKEN
			|| peek().type == token::END_CALLBACK
			|| END_STATEMENTS.contains(peek().val);
	};
	consume(); // consume <
	_skip_linebreaks();

	if (peek().type == token::GREATER_THAN) {
		auto error = Diagnostic(ErrorType::ParseError, "", "", peek());
		error.message = "Parameterized type <" + type_token.val
			+ "> must provide at least one type argument.";
		error.expected = "a type argument such as <int> or <Note>";
		return Result<std::vector<Type*>>(error);
	}

	while (true) {
		_skip_linebreaks();
		if (reached_construct_end()) {
			auto error = Diagnostic(ErrorType::ParseError, "", "", peek());
			error.message = "The type argument list for parameterized type <"
				+ type_token.val + "> is not closed.";
			error.expected = ">";
			return Result<std::vector<Type*>>(error);
		}
		if (peek().type == token::COMMA) {
			auto error = Diagnostic(ErrorType::ParseError, "", "", peek());
			error.message = "Expected a type argument before <,> in parameterized type <"
				+ type_token.val + ">.";
			error.expected = "a type such as <int> or <Note>";
			return Result<std::vector<Type*>>(error);
		}
		if (peek().type != token::KEYWORD) {
			auto error = Diagnostic(ErrorType::ParseError, "", "", peek());
			error.message = "Expected a type argument in parameterized type <"
				+ type_token.val + ">.";
			error.expected = "a type such as <int> or <Note>";
			return Result<std::vector<Type*>>(error);
		}

		auto argument = _parse_single_types(references, false);
		if (argument.is_error()) return Result<std::vector<Type*>>(argument.get_error());
		arguments.push_back(argument.unwrap());
		_skip_linebreaks();

		if (peek().type == token::GREATER_THAN) {
			consume();
			return Result<std::vector<Type*>>(std::move(arguments));
		}
		if (peek().type == token::COMMA) {
			consume();
			_skip_linebreaks();
			if (peek().type == token::GREATER_THAN) {
				consume();
				return Result<std::vector<Type*>>(std::move(arguments));
			}
			continue;
		}

		auto error = Diagnostic(ErrorType::ParseError, "", "", peek());
		if (peek().type == token::KEYWORD) {
			error.message = "Missing comma between type arguments <"
				+ arguments.back()->to_string() + "> and <" + peek().val
				+ "> in parameterized type <" + type_token.val + ">.";
		} else {
			error.message = "The type argument list for parameterized type <"
				+ type_token.val + "> is not closed.";
		}
		error.expected = ", or >";
		return Result<std::vector<Type*>>(error);
	}
}

Result<Type*> Processor::_parse_function_type(TypeReferences* references) {
	std::vector<Type*> params;
	// consume the open parenthesis
	consume();
	while(peek().type != token::CLOSED_PARENTH) {
		auto type = parse_type(references);
		if(type.is_error()) return Result<Type*>(type.get_error());
		params.push_back(type.unwrap());
		if(peek().type == token::COMMA) {
			consume();
		}
	}
	consume(); // consume the closed parenthesis
	if(peek().type != token::TYPE) {
		auto error = Diagnostic(ErrorType::ParseError,"", "", get_tok());
		error.message = "Found incorrect Function type syntax. Expected return type annotation.";
		error.actual = peek().val;
		error.expected = ":";
		return Result<Type*>(error);
	}
	consume(); // consume the colon
	auto return_type = parse_type(references);
	if(return_type.is_error()) return Result<Type*>(return_type.get_error());
	return Result<Type*>(TypeRegistry::add_function_type(params, return_type.unwrap()));
}

void Processor::_skip_linebreaks() {
	while(peek().type == token::LINEBRK){
		consume();
	}
}

NodeStruct* Processor::current_struct() const {
	return m_struct_stack.empty() ? nullptr : m_struct_stack.back();
}
