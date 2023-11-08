//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorIncrementer : public Preprocessor {
public:
	PreprocessorIncrementer(std::vector<Token> tokens, const std::string &currentFile);
	Result<SuccessTag> process_incrementations();

private:


	Result<SuccessTag> parse_start_inc(const std::vector<Token>& tok);
	Result<SuccessTag> parse_end_inc(const std::vector<Token>& tok);
};

