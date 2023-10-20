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
            auto define = parse_define_statement(m_tokens);
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
//	for(auto &def : m_define_statements) {
//		evaluate_define_statements(def->assignee);
//	}

	auto evaluation_result = evaluate_define_statements(m_tokens);
	if(evaluation_result.is_error())
		return Result<SuccessTag>(evaluation_result.get_error());

    return Result<SuccessTag>(SuccessTag{});
}


Result<SuccessTag> PreprocessorDefine::evaluate_define_statements(std::vector<Token>& tok) {
	m_pos = 0;
	while (peek(tok).type != token::END_TOKEN) {
		if(peek(tok).type == KEYWORD and m_pos > 0 and (peek(tok, -1).type != MACRO or peek(tok, -1).type != FUNCTION)) {
			auto define_stmt = get_define_statement(peek(tok).val);
			if(define_stmt != nullptr) {
				auto evaluation_result = evaluate_define_statement(tok, define_stmt);
				if(evaluation_result.is_error())
					return Result<SuccessTag>(evaluation_result.get_error());
			} else consume(tok);
		} else consume(tok);
	}
	return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorDefine::evaluate_define_statement(std::vector<Token>& tok, std::unique_ptr<NodeDefineStatement>* define_stmt) {
	std::vector<std::vector<Token>> new_args = {};
	size_t og_pos = m_pos;
	if(peek(tok, 1).type == OPEN_PARENTH) {
		auto defined_stmt = parse_define_header(tok);
		if(defined_stmt.is_error())
			return Result<SuccessTag>(defined_stmt.get_error());
		new_args = defined_stmt.unwrap()->args;
	} else
		tok.erase(tok.begin() + m_pos);

	std::vector<Token> replace = (*define_stmt)->assignee;
	auto placeholders = (*define_stmt)->to_be_defined->args;
	for(auto &r : replace) {
		r.line = peek(tok).line;
		r.file = peek(tok).file;
	}

	if(new_args.size() != placeholders.size())
		return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	 "Found different numbers in define constant call than originally defined. Maybe there exists a macro with the same name?",
	 peek(tok).line,std::to_string(placeholders.size()),std::to_string(new_args.size()), peek(tok).file));

	if (!new_args.empty()) {
		auto substitution_result = substitute_macro_params(replace, placeholders, new_args);
		if (substitution_result.is_error())
			return Result<SuccessTag>(substitution_result.get_error());
	}

//	if(has_recursive_macro_calls) {
		// add end_token so that process_macro_calls while loop stops
		replace.emplace_back(END_TOKEN, "", 0, m_current_file);
		auto define_call_result = evaluate_define_statements(replace);
		if (define_call_result.is_error())
			return Result<SuccessTag>(define_call_result.get_error());
		if (replace.back().type == token::END_TOKEN) replace.pop_back(); // delete end_token
//	}
	m_pos = og_pos;
	// insert macro_body into tok at position of define call
	tok.insert(tok.begin() + m_pos, replace.begin(), replace.end());
	m_pos += replace.size();
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


Result<std::unique_ptr<NodeDefineStatement>> PreprocessorDefine::parse_define_statement(std::vector<Token>& tok) {
	size_t begin = m_pos;

    consume(tok); //consume define
	if (peek(tok).type != token::KEYWORD) {
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
		 "Missing define name.",peek(tok).line,"keyword",peek(tok).val, peek(tok).file));
	}
	auto define_header_result = parse_define_header(tok);
	if(define_header_result.is_error())
		return Result<std::unique_ptr<NodeDefineStatement>>(define_header_result.get_error());

	if(peek(tok).type != ASSIGN)
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
 	"Found invalid Define Statement Syntax. Missing <assign> symbol.", peek(tok).line, ":=", peek(tok).val, peek(tok).file));
	consume(tok); //consume :=

	std::vector<Token> assignee = {};
	while(peek(tok).type != LINEBRK) {
		if (peek(tok).type == token::END_TOKEN)
			return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
			"Unexpected end of tokens. Missing assignment of define statement.",peek(tok).line, "", peek(tok).val,peek(tok).file));
		assignee.push_back(consume(tok));
	}
	if(assignee.empty()) {
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	 "Found empty define statement assignment.",peek(tok).line, "", peek(tok).val,peek(tok).file));
	}
	if (peek(tok).type != token::LINEBRK) {
		return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	 	"Missing necessary linebreak after define statement.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
	}
	for(const auto &ass : assignee) {
		if(ass.type == KEYWORD and define_header_result.unwrap()->name == ass.val) {
			return Result<std::unique_ptr<NodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	 "A define constant cannot define itself.",peek(tok).line,"","", peek(tok).file));
		}
	}
	consume(tok); //consume linebreak

	remove_tokens(tok, begin, m_pos);
	auto define_statement = std::make_unique<NodeDefineStatement>(std::move(define_header_result.unwrap()), std::move(assignee));
	return Result<std::unique_ptr<NodeDefineStatement>>(std::move(define_statement));
}

Result<std::unique_ptr<NodeDefineHeader>> PreprocessorDefine::parse_define_header(std::vector<Token>& tok) {
	size_t begin = m_pos;

	Token define = consume(tok);
	std::string define_name = define.val;

	auto define_args_result = parse_nested_params_list(tok);
	if(define_args_result.is_error())
		return Result<std::unique_ptr<NodeDefineHeader>>(define_args_result.get_error());

	remove_tokens(tok, begin, m_pos);

	auto value = std::make_unique<NodeDefineHeader>(define_name, std::move(define_args_result.unwrap()), define);
	return Result<std::unique_ptr<NodeDefineHeader>>(std::move(value));
}


