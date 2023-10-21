//
// Created by Mathias Vatter on 13.10.23.
//

#pragma once

#include "Preprocessor.h"

class PreprocessorDefine : public Preprocessor {
public:
    PreprocessorDefine(const std::vector<Token> &tokens, const std::string &currentFile);
    Result<SuccessTag> process_defines();

private:

	Result<SuccessTag> process_define_definitions();
//	Result<SuccessTag> process_define_call();

	Result<SuccessTag> process_define_calls(std::vector<Token>& tok);
	Result<SuccessTag> evaluate_define_statement(std::vector<Token>& tok, std::unique_ptr<NodeDefineStatement>* define_stmt);
    bool is_define_call(const std::vector<Token>& tok);

	std::unique_ptr<NodeDefineStatement>* get_define_statement(const std::string &name);

	Result<std::unique_ptr<NodeDefineStatement>> parse_define_statement(std::vector<Token>& tok);
	Result<std::unique_ptr<NodeDefineHeader>> parse_define_header(std::vector<Token>& tok);


};


