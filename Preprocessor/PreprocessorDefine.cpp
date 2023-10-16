//
// Created by Mathias Vatter on 13.10.23.
//

#include "PreprocessorDefine.h"

PreprocessorDefine::PreprocessorDefine(const std::vector<Token> &tokens, const std::string &currentFile) : Preprocessor(
        tokens, currentFile) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
}

Result<SuccessTag> PreprocessorDefine::process_defines() {
    m_pos = 0;
    // parse defines
    while (peek(m_tokens).type != token::END_TOKEN) {
        if(peek(m_tokens).type == token::DEFINE) {
            auto define = parse_define_statement();
            if(define.is_error())
                return Result<SuccessTag>(define.get_error());
			if(search(m_define_strings, define.unwrap()->to_be_defined->name) != -1)
				return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
				 "Define constant has already been defined.",peek(m_tokens).line,"",define.unwrap()->to_be_defined->name, peek(m_tokens).file));
			m_define_strings.push_back(define.unwrap()->to_be_defined->name);
            m_define_statements.push_back(std::move(define.unwrap()));
        } else {
            consume(m_tokens);
        }
    }
	auto evaluation_result = evaluate_define_statements();
	if(evaluation_result.is_error())
		return Result<SuccessTag>(evaluation_result.get_error());

    return Result<SuccessTag>(SuccessTag{});
}


Result<SuccessTag> PreprocessorDefine::evaluate_define_statements() {
	m_pos = 0;
	while (peek(m_tokens).type != token::END_TOKEN) {
		if(peek(m_tokens).type == KEYWORD and peek(m_tokens, -1).type != MACRO) {
			auto define_stmt = get_define_statement(peek(m_tokens).val);
			if(define_stmt != nullptr) {
				auto evaluation_result = evaluate_define_statement(define_stmt);
				if(evaluation_result.is_error())
					return Result<SuccessTag>(evaluation_result.get_error());
			} else consume(m_tokens);
		} else consume(m_tokens);
	}
	return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorDefine::evaluate_define_statement(std::unique_ptr<NodeDefineStatement>* define_stmt) {
	std::vector<std::vector<Token>> new_args = {};
	if(peek(m_tokens, 1).type == OPEN_PARENTH) {
		auto defined_stmt = parse_define_header();
		if(defined_stmt.is_error())
			return Result<SuccessTag>(defined_stmt.get_error());
		new_args = defined_stmt.unwrap()->args;
	}
	std::vector<Token> replace = (*define_stmt)->assignee;
	auto placeholders = (*define_stmt)->to_be_defined->args;
	for(auto &r : replace) {
		r.line = peek(m_tokens).line;
		r.file = peek(m_tokens).file;
	}

	if(new_args.size() != placeholders.size())
		return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	 "Found different numbers in define constant call than originally defined. Maybe there exists a macro with the same name?",
	 peek(m_tokens).line,std::to_string(placeholders.size()),std::to_string(new_args.size()), peek(m_tokens).file));

	if (!new_args.empty()) {
		auto substitution_result = substitute_macro_params(replace, placeholders, new_args);
		if (substitution_result.is_error())
			return Result<SuccessTag>(substitution_result.get_error());
	}

	// insert macro_body into tok at position of define call
	m_tokens.insert(m_tokens.begin() + m_pos, replace.begin(), replace.end());
	m_pos += replace.size();
	m_tokens.erase(m_tokens.begin() + m_pos);
	return Result<SuccessTag>(SuccessTag{});
}


std::unique_ptr<NodeDefineStatement>* PreprocessorDefine::get_define_statement(const std::string &name) {
	for(int i = 0; i<m_define_strings.size(); i++) {
		if(m_define_strings[i] == name) {
			return &m_define_statements[i];
		}
	}
	return nullptr;
}


Result<std::unique_ptr<NodeDefineStatement>> PreprocessorDefine::parse_define_statement() {
	size_t begin = m_pos;

    consume(m_tokens); //consume define
	if (peek(m_tokens).type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
		 "Missing define name.",peek(m_tokens).line,"keyword",peek(m_tokens).val, peek(m_tokens).file));
	}
	auto define_header_result = parse_define_header();
	if(define_header_result.is_error())
		return Result<std::unique_ptr<NodeDefineStatement>>(define_header_result.get_error());

	if(peek(m_tokens).type != ASSIGN)
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
 	"Found invalid Define Statement Syntax. Missing <assign> symbol.", peek(m_tokens).line, ":=", peek(m_tokens).val, peek(m_tokens).file));
	consume(m_tokens); //consume :=

	std::vector<Token> assignee = {};
	while(peek(m_tokens).type != LINEBRK) {
		if (peek(m_tokens).type == token::END_TOKEN)
			return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
			"Unexpected end of tokens. Missing assignment of define statement.",peek(m_tokens).line, "", peek(m_tokens).val,peek(m_tokens).file));
		assignee.push_back(consume(m_tokens));
	}
	if(assignee.empty()) {
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	 "Found empty define statement assignment.",peek(m_tokens).line, "", peek(m_tokens).val,peek(m_tokens).file));
	}
	if (peek(m_tokens).type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	 	"Missing necessary linebreak after define statement.",peek(m_tokens).line,"linebreak",peek(m_tokens).val, peek(m_tokens).file));
	}
	for(const auto &ass : assignee) {
		if(ass.type == KEYWORD and define_header_result.unwrap()->name == ass.val) {
			return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	 "A define constant cannot define itself.",peek(m_tokens).line,"","", peek(m_tokens).file));
		}
	}
	consume(m_tokens); //consume linebreak

	remove_tokens(m_tokens, begin, m_pos);
	auto define_statement = std::make_unique<NodeDefineStatement>(std::move(define_header_result.unwrap()), std::move(assignee));
	return Result<std::unique_ptr<NodeDefineStatement>>(std::move(define_statement));
}

Result<std::unique_ptr<NodeDefineHeader>> PreprocessorDefine::parse_define_header() {
	Token define = consume(m_tokens);
	std::string define_name = define.val;

	auto define_args_result = parse_nested_params_list(m_tokens);
	if(define_args_result.is_error())
		return Result<std::unique_ptr<NodeDefineHeader>>(define_args_result.get_error());

	auto value = std::make_unique<NodeDefineHeader>(define_name, std::move(define_args_result.unwrap()), define);
	return Result<std::unique_ptr<NodeDefineHeader>>(std::move(value));
}


