//
// Created by Mathias Vatter on 08.11.23.
//

#include "PreprocessorIncrementer.h"

PreprocessorIncrementer::PreprocessorIncrementer(std::vector<Token> tokens, const std::string &currentFile) :
	Preprocessor(std::move(tokens), currentFile) {
	m_pos = 0;
	m_curr_token = m_tokens.at(0).type;
}

Result<SuccessTag> PreprocessorIncrementer::process_incrementations() {

}

Result<SuccessTag> PreprocessorIncrementer::parse_start_inc(const std::vector<Token> &tok) {
	consume(tok); // consume START_INC
	if(peek(tok).type != OPEN_PARENTH) {
		return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	 "Found invalid <START_INC> syntax.",peek(tok).line,"(",peek(tok).val, peek(tok).file));
	}
	consume(tok); // consume (
	if(peek(tok).type != KEYWORD) {
		return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	 "Found invalid <START_INC> syntax.",peek(tok).line,"<keyword>",peek(tok).val, peek(tok).file));
	}
	auto replace = consume(tok); // consume keyword

	if(peek(tok).type != LINEBRK) {
		return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
											   "Found invalid <condition> syntax after <END_USE_CODE>.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
	}
	consume(tok); // consume linebreak
}
