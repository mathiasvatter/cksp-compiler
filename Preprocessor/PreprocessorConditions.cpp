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
    return Result<SuccessTag>(SuccessTag());
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
            auto it = std::find(vec.begin(), vec.end(), condition_definition.unwrap());
            // Überprüfen, ob der Wert gefunden wurde
            if (it != vec.end()) {
                // Löschen des Werts aus dem vector
                vec.erase(it);
            }
        } else {
            consume(m_tokens);
        }
    }

    return Result<SuccessTag>(SuccessTag{});
}

Result<Token> PreprocessorConditions::parse_condition_definition(std::vector<Token>& tok) {
    consume(tok); // consume SET_CONDITION
    if(peek(tok).type != OPEN_PARENTH) {
        return Result<Token>(CompileError(ErrorType::SyntaxError,
	  	"Found invalid <condition> syntax.",peek(tok).line,"(",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume (
    if(peek(tok).type != KEYWORD) {
        return Result<Token>(CompileError(ErrorType::SyntaxError,
      "Found invalid <condition> syntax.",peek(tok).line,"<condition-symbol>",peek(tok).val, peek(tok).file));
    }
    auto condition = consume(tok);
    if(peek(tok).type != CLOSED_PARENTH) {
        return Result<Token>(CompileError(ErrorType::SyntaxError,
    "Found invalid <condition> syntax.",peek(tok).line,")",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume )
    if(peek(tok).type != LINEBRK) {
        return Result<Token>(CompileError(ErrorType::SyntaxError,
    "Found invalid <condition> syntax.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume linebreak
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
