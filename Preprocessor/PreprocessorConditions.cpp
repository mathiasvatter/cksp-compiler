//
// Created by Mathias Vatter on 08.11.23.
//

#include "PreprocessorConditions.h"

PreprocessorConditions::PreprocessorConditions(std::vector<Token> tokens, const std::string &currentFile) :
        Preprocessor(std::move(tokens), currentFile) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
}

Result<SuccessTag> PreprocessorConditions::process_conditions() {
	auto condition_definitions = get_conditions();
	if(condition_definitions.is_error())
		return Result<SuccessTag>(condition_definitions.get_error());

	auto condition_evaluation = evaluate_conditions();
	if(condition_evaluation.is_error())
		return Result<SuccessTag>(condition_evaluation.get_error());

	return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorConditions::get_conditions() {
    m_pos = 0;
    // parse macro definitions
    while (peek(m_tokens).type != token::END_TOKEN) {
        if(is_condition_definition(m_tokens, peek(m_tokens), SET_CONDITION)) {
            auto condition_definition = parse_condition_definition(m_tokens);
            if (condition_definition.is_error())
                return Result<SuccessTag>(condition_definition.get_error());
            m_conditions.push_back(std::move(condition_definition.unwrap()));
        } else if (is_condition_definition(m_tokens, peek(m_tokens), RESET_CONDITION)) {
            auto condition_definition = parse_condition_definition(m_tokens);
            if (condition_definition.is_error())
                return Result<SuccessTag>(condition_definition.get_error());
            reset_condition(condition_definition.unwrap());
        } else {
            consume(m_tokens);
        }
    }
    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorConditions::evaluate_conditions() {
	m_pos = 0;
	while(peek(m_tokens).type != token::END_TOKEN) {
		if(is_beginning_of_line_keyword(m_tokens, USE_CODE_IF_NOT) or is_beginning_of_line_keyword(m_tokens, USE_CODE_IF)) {
			auto use_code_result = parse_use_code_if(m_tokens);
			if(use_code_result.is_error())
				Result<SuccessTag>(use_code_result.get_error());
			bool use_code = use_code_result.unwrap();
			size_t begin = m_pos;
			while(peek(m_tokens).type != END_USE_CODE || peek(m_tokens, -1).type != LINEBRK) {
				if(peek(m_tokens).type == END_USE_CODE) {
					return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
				   "Missing linebreak before <END_USE_CODE>.",peek(m_tokens).line,"linebreak",peek(m_tokens).val, peek(m_tokens).file));
				}
				if(peek(m_tokens).type == END_TOKEN) {
					return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
				 	"Missing <END_USE_CODE>. Reached end_of_file.",peek(m_tokens).line,"<END_USE_CODE>",peek(m_tokens).val, peek(m_tokens).file));
				}
				consume(m_tokens);
			}
			auto end_use_code_result = parse_end_use_code(m_tokens);
			if(end_use_code_result.is_error())
				Result<SuccessTag>(end_use_code_result.get_error());
			if(!use_code) remove_tokens(m_tokens, begin, m_pos);
		} else consume(m_tokens);
	}
	return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorConditions::parse_end_use_code(std::vector<Token>& tok) {
	size_t begin = m_pos;
	consume(tok); // consume END_USE_CODE
	if(peek(tok).type != LINEBRK) {
		return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	   "Found invalid <condition> syntax after <END_USE_CODE>.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
	}
	consume(tok); // consume linebreak
	remove_tokens(m_tokens, begin, m_pos);
	return Result<SuccessTag>(SuccessTag{});
}


Result<bool> PreprocessorConditions::parse_use_code_if(std::vector<Token>& tok) {
	size_t begin = m_pos;
	auto token = consume(tok); // consume USE_CODE_IF/_NOT
	if(peek(tok).type != OPEN_PARENTH) {
		return Result<bool>(CompileError(ErrorType::PreprocessorError,
	  "Found invalid <USE_CODE_> syntax.",peek(tok).line,"(",peek(tok).val, peek(tok).file));
	}
	consume(tok); // consume (
	if(peek(tok).type != KEYWORD) {
		return Result<bool>(CompileError(ErrorType::PreprocessorError,
  "Found invalid <condition> syntax.",peek(tok).line,"<condition-symbol>",peek(tok).val, peek(tok).file));
	}
	auto condition = consume(tok);
	bool condition_is_set = true;
	auto it = std::find(m_conditions.begin(), m_conditions.end(), condition);
	if (it == m_conditions.end()) {
		condition_is_set = false;
	}
	if(peek(tok).type != CLOSED_PARENTH) {
		return Result<bool>(CompileError(ErrorType::PreprocessorError,
	  "Found invalid <condition> syntax.",peek(tok).line,")",peek(tok).val, peek(tok).file));
	}
	consume(tok); // consume )
	if(peek(tok).type != LINEBRK) {
		return Result<bool>(CompileError(ErrorType::PreprocessorError,
	  "Found invalid <condition> syntax.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
	}
	consume(tok); // consume linebreak
	remove_tokens(tok, begin, m_pos);
	if ((token.type == USE_CODE_IF and condition_is_set) or (token.type == USE_CODE_IF_NOT and !condition_is_set)) {
		return Result<bool>(true);
	}
	return Result<bool>(false);
}



Result<Token> PreprocessorConditions::parse_condition_definition(std::vector<Token>& tok) {
	size_t begin = m_pos;
    consume(tok); // consume SET_CONDITION
    if(peek(tok).type != OPEN_PARENTH) {
        return Result<Token>(CompileError(ErrorType::PreprocessorError,
	  	"Found invalid <condition> syntax.",peek(tok).line,"(",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume (
    if(peek(tok).type != KEYWORD) {
        return Result<Token>(CompileError(ErrorType::PreprocessorError,
      "Found invalid <condition> syntax.",peek(tok).line,"<condition-symbol>",peek(tok).val, peek(tok).file));
    }
    auto condition = consume(tok);
    if(peek(tok).type != CLOSED_PARENTH) {
        return Result<Token>(CompileError(ErrorType::PreprocessorError,
    "Found invalid <condition> syntax.",peek(tok).line,")",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume )
    if(peek(tok).type != LINEBRK) {
        return Result<Token>(CompileError(ErrorType::PreprocessorError,
    "Found invalid <condition> syntax.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume linebreak
	remove_tokens(tok, begin, m_pos);
    return Result<Token>(condition);
}

bool PreprocessorConditions::is_beginning_of_line_keyword(const std::vector<Token>& tok, token token) {
    if (m_pos > 0)
        return (peek(tok, -1).type == token::LINEBRK and peek(tok, 0).type == token);
    else
        return m_pos == 0 and peek(tok).type == token;
}

bool PreprocessorConditions::is_builtin_condition(const std::vector<Token> &tok, const Token& token) {
    return (contains(builtin_conditions, token.val));
}

bool PreprocessorConditions::is_condition_definition(const std::vector<Token> &tok, const Token &peek, token token_type) {
    return peek.type == token_type and is_beginning_of_line_keyword(tok, peek.type) and not is_builtin_condition(tok, peek);
}

void PreprocessorConditions::reset_condition(const Token& condition) {
	auto it = std::find(m_conditions.begin(), m_conditions.end(), condition);
	if (it != m_conditions.end()) {
		m_conditions.erase(it);
	}
}
