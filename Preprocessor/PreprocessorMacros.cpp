//
// Created by Mathias Vatter on 11.10.23.
//

#include "PreprocessorMacros.h"

PreprocessorMacros::PreprocessorMacros(std::vector<Token> tokens, const std::string &currentFile)
	: Preprocessor(std::move(tokens),currentFile) {
	m_pos = 0;
	m_curr_token = m_tokens.at(0).type;
}


Result<SuccessTag> PreprocessorMacros::process_macros() {
    auto processed_macro_defs = process_macro_definitions();
    if(processed_macro_defs.is_error())
        return Result<SuccessTag>(processed_macro_defs.get_error());

    auto processed_macro_calls = process_macro_calls(m_tokens);
    if(processed_macro_calls.is_error())
        return Result<SuccessTag>(processed_macro_calls.get_error());
    return Result<SuccessTag>(processed_macro_calls.unwrap());
}

Result<SuccessTag> PreprocessorMacros::process_macro_definitions() {
    m_pos = 0;
    // parse macro definitions
    while (peek(m_tokens).type != token::END_TOKEN) {
        if(peek(m_tokens).type == token::MACRO) {
            auto macro_definition = parse_macro_definition(m_tokens);
            if(macro_definition.is_error())
                return Result<SuccessTag>(macro_definition.get_error());
            else
                m_macro_definitions.push_back(std::move(macro_definition.unwrap()));
        } else {
            consume(m_tokens);
        }
    }

    return Result<SuccessTag>(SuccessTag{});
}

bool PreprocessorMacros::is_replacement_macro(const std::vector<std::vector<Token>>& args) {
	for (auto &arg : args) {
		for (auto &tok : arg) {
			if (tok.type == KEYWORD && count_char(tok.val, '#') == 2)
				return true;
		}
	}
	return false;
}


Result<SuccessTag> PreprocessorMacros::process_macro_calls(std::vector<Token>& tok) {
	std::vector<std::unique_ptr<NodeMacroHeader>> macro_calls;
    m_pos = 0;
    while (peek(tok).type != token::END_TOKEN) {
        if(is_macro_call(tok)) {
            if (is_defined_macro(peek(tok).val)) {
                auto macro_call = parse_macro_call(tok);
                if (macro_call.is_error())
                    return Result<SuccessTag>(macro_call.get_error());
//				macro_calls.push_back(std::move(macro_call.unwrap()));
				auto macro_call_evaluation = evaluate_macro_call(std::move(macro_call.unwrap()), tok);
				if (macro_call_evaluation.is_error())
					return Result<SuccessTag>(macro_call_evaluation.get_error());
            } else consume(tok);
        } else if (is_beginning_of_line_keyword(tok, ITERATE_MACRO) or is_beginning_of_line_keyword(tok, LITERATE_MACRO)) {
            auto macro_call_list = Result<std::vector<std::unique_ptr<NodeMacroHeader>>>({});
            if(peek(tok).type == ITERATE_MACRO)
                macro_call_list = parse_iterate_macro(tok);
            else if (peek(tok).type == LITERATE_MACRO)
                macro_call_list = parse_literate_macro(tok);
            if (macro_call_list.is_error())
                return Result<SuccessTag>(macro_call_list.get_error());

            for(auto &macro_call : macro_call_list.unwrap()) {
				auto macro_call_evaluation = evaluate_macro_call(std::move(macro_call), tok);
				if (macro_call_evaluation.is_error())
					return Result<SuccessTag>(macro_call_evaluation.get_error());
			}
        } else consume(tok);
    }

    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorMacros::evaluate_macro_call(std::unique_ptr<NodeMacroHeader> macro_call, std::vector<Token>& tok) {
    std::vector<Token> macro_body;
    std::vector<std::vector<Token>> macro_params = {};
    bool has_recursive_macro_calls;
    bool valid_macro_call = false;
    for(auto &macro_def : m_macro_definitions) {
        if(macro_call->name == macro_def->header->name and macro_call->args.size() == macro_def->header->args.size()) {
            has_recursive_macro_calls = macro_def->has_recursive_calls;
            macro_body = macro_def->body;
            macro_params = macro_def->header->args;
            valid_macro_call = true;
            break;
        }
    }

    // return false if no macro definition found has the necessary amount of params of called macro
    if(!valid_macro_call)
        return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	   "Found incorrect number of parameters in macro call.", macro_call->tok.line, "valid number of parameters", std::to_string(macro_call->args.size()), macro_call->tok.file));

    // check if called macro is already being evaluated at the moment -> recursive macro call
    if(std::find(m_macro_evaluation_stack.begin(), m_macro_evaluation_stack.end(), macro_call->name) != m_macro_evaluation_stack.end()) {
        // recursive macro call detected
        return Result<SuccessTag>(CompileError(ErrorType::PreprocessorError,
	   "Recursive macro call detected. Calling the macro inside its definition is not allowed.", macro_call->tok.line, "", macro_call->name, macro_call->tok.file));
    }
    m_macro_evaluation_stack.push_back(macro_call->name);


    // substitution
	auto substitution_result = substitute_macro_params(macro_body, macro_params, macro_call->args);
	if(substitution_result.is_error()) {
		return substitution_result;
	}
    if(has_recursive_macro_calls) {
        // add end_token so that process_macro_calls while loop stops
        macro_body.emplace_back(END_TOKEN, "", 0, m_current_file);
        auto macro_call_result = process_macro_calls(macro_body);
        if (macro_call_result.is_error())
            return Result<SuccessTag>(macro_call_result.get_error());
        if (macro_body.back().type == token::END_TOKEN) macro_body.pop_back(); // delete end_token
    }
	// alter m_pos to the position of the start of the macro_call (since all macro_call tokens got deleted in parse_macro_call)
    m_pos = macro_call->token_pos;
	// insert macro_body into tok at position of macro_call
    tok.insert(tok.begin() + m_pos, macro_body.begin(), macro_body.end());
    m_pos += macro_body.size();
	// delete current macro name from evaluation stack
    m_macro_evaluation_stack.pop_back();
    return Result<SuccessTag>(SuccessTag{});
}

Result<SuccessTag> PreprocessorMacros::substitute_macro_params(std::vector<Token>& macro_body, const std::vector<std::vector<Token>>& placeholders, const std::vector<std::vector<Token>>& new_args) {
	for(int i = 0; i < placeholders.size(); i++) {
		// if the foo in macro(foo) is going to be replaced with foo anyways do not bother substituting -> as it ends in infinite loop
		if(not(new_args[i].size() == 1 and new_args[i][0].val == placeholders[i][0].val))
			for (size_t j = 0; j < macro_body.size(); ) {
				// find keyword from params in body
				if(placeholders[i][0].val == macro_body[j].val) {
					// Füge die Tokens aus new_args[i] vor dem aktuellen Token ein
					macro_body.insert(macro_body.begin() + j, new_args[i].begin(), new_args[i].end());
					// Lösche das aktuelle Token
					macro_body.erase(macro_body.begin() + j + new_args[i].size());
				} else if(count_char(placeholders[i][0].val, '#') == 2 and contains(macro_body[j].val, placeholders[i][0].val)) {
					std::string hashtag_replacement;
					for(auto & arg : new_args[i]) {
						hashtag_replacement += arg.val;
					}
					macro_body[j].val.replace(macro_body[j].val.find(placeholders[i][0].val), placeholders[i][0].val.size(), hashtag_replacement);
				} else {
					++j;
				}
			}
	}
	return Result<SuccessTag>(SuccessTag{});
}

bool PreprocessorMacros::is_macro_call(const std::vector<Token>& tok) {
	if(m_pos > 0)
		return peek(tok, -1).type == LINEBRK && peek(tok, 0).type == token::KEYWORD && (peek(tok, 1).type == token::LINEBRK xor peek(tok, 1).type == token::OPEN_PARENTH);
	else
		return m_pos == 0 && peek(tok, 0).type == token::KEYWORD && (peek(tok, 1).type == token::LINEBRK xor peek(tok, 1).type == token::OPEN_PARENTH);
}

bool PreprocessorMacros::is_beginning_of_line_keyword(const std::vector<Token>& tok, token token) {
	if(m_pos>0)
		return (peek(tok, -1).type == token::LINEBRK and peek(tok, 0).type == token);
	else
		return m_pos == 0 and peek(tok).type == token;
}


bool PreprocessorMacros::is_defined_macro(const std::string &name) {
    for(auto &macro_def : m_macro_definitions) {
        if(name == macro_def->header->name) {
            return true;
        }
    }
    return false;
}

Result<std::unique_ptr<NodeMacroHeader>> PreprocessorMacros::parse_macro_header(std::vector<Token>& tok) {
    std::vector<std::vector<Token>> macro_args = {};
    size_t start_pos = m_pos;
    Token macro = consume(tok);
    std::string macro_name = macro.val;

    if (peek(tok).type == token::OPEN_PARENTH) {
        consume(tok); // consume (

        if (peek(tok).type != token::CLOSED_PARENTH) {
            int parenth_depth = 1; // Start with 1 because we've already consumed the first OPEN_PARENTH
            while (parenth_depth > 0) {
                if (peek(tok).type == token::OPEN_PARENTH) {
                    parenth_depth++;
                } else if (peek(tok).type == token::CLOSED_PARENTH) {
                    parenth_depth--;
                } else if (peek(tok).type == token::END_TOKEN) {
                    return Result<std::unique_ptr<NodeMacroHeader>>(CompileError(ErrorType::SyntaxError,
                     "Unexpected end of tokens. Missing closing parenthesis.",peek(tok).line, ")", peek(tok).val,peek(tok).file));
                }
                std::vector<Token> arg = {};
                while (peek(tok).type != token::COMMA and parenth_depth > 0) {
                    arg.push_back(consume(tok));
                    if (peek(tok).type == token::OPEN_PARENTH) {
                        parenth_depth++;
                    } else if (peek(tok).type == token::CLOSED_PARENTH) {
                        parenth_depth--;
                    }
                }
                if (peek(tok).type == token::COMMA && parenth_depth > 0) {
                    consume(tok); // consume COMMA only if we're not at the outermost parenthesis level
                }
                macro_args.push_back(arg);
            }
            if (peek(tok).type != token::CLOSED_PARENTH) {
                return Result<std::unique_ptr<NodeMacroHeader>>(CompileError(ErrorType::SyntaxError,
             "Missing closing parenthesis.",peek(tok).line, ")", peek(tok).val,peek(tok).file));
            }
        }
        consume(tok); //consume )
    }

    auto value = std::make_unique<NodeMacroHeader>(macro_name, std::move(macro_args), start_pos, macro);
    return Result<std::unique_ptr<NodeMacroHeader>>(std::move(value));
}


Result<std::unique_ptr<NodeMacroDefinition>> PreprocessorMacros::parse_macro_definition(std::vector<Token>& tok) {
    std::unique_ptr<NodeMacroHeader> macro_header;
    std::vector<Token> macro_body;
    size_t begin = m_pos;
    consume(tok); //consume "macro"
    if (peek(tok).type != token::KEYWORD) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
	 "Missing macro name.",peek(tok).line,"keyword",peek(tok).val, peek(tok).file));
    }
    auto header = parse_macro_header(tok);
    if (header.is_error()) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(header.get_error());
    }
    macro_header = std::move(header.unwrap());
    if (peek(tok).type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
	 "Missing necessary linebreak after macro header.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume linebreak
    bool has_recursive_calls = false;
    while (peek(tok).type != token::END_MACRO) {
        has_recursive_calls &= (is_macro_call(tok) or is_beginning_of_line_keyword(tok, ITERATE_MACRO) or
                is_beginning_of_line_keyword(tok, LITERATE_MACRO));
        if(peek(tok).type == token::MACRO)
            return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
		 "Nested macros are not allowed. Maybe you forgot an 'end macro' along the line?",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
        macro_body.push_back(consume(tok));
    }
    consume(tok); // consume end macro
    if (peek(tok).type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
	 "Missing necessary linebreak after 'end macro'.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume linebreak
    remove_tokens(tok, begin, m_pos);
    for(auto &arg : macro_header->args) {
        if(not(arg.size() == 1 and arg.at(0).type == token::KEYWORD))
            return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
		 "Only keywords allowed as parameters when defining macros.",arg.at(0).line,"keyword",arg.at(0).val, arg.at(0).file));
    }
    auto value = std::make_unique<NodeMacroDefinition>(std::move(macro_header), std::move(macro_body), has_recursive_calls, get_tok(tok));
    return Result<std::unique_ptr<NodeMacroDefinition>>(std::move(value));
}

Result<std::unique_ptr<NodeMacroHeader>> PreprocessorMacros::parse_macro_call(std::vector<Token>& tok) {
    size_t begin = m_pos;
    auto macro_header = parse_macro_header(tok);
    if(macro_header.is_error())
        return Result<std::unique_ptr<NodeMacroHeader>>(macro_header.get_error());
    if (peek(tok).type != token::LINEBRK) {
        return Result<std::unique_ptr<NodeMacroHeader>>(CompileError(ErrorType::SyntaxError,
		 "Missing necessary linebreak after macro call. Maybe you forgot a closing parenthesis?",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); // consume linebreak
    remove_tokens(tok, begin, m_pos);
    return std::move(macro_header);
}

Result<std::vector<std::unique_ptr<NodeMacroHeader>>> PreprocessorMacros::parse_iterate_macro(std::vector<Token>& tok) {
    size_t begin = m_pos;
    consume(tok); // consume iterate_macro
    if(peek(tok).type != token::OPEN_PARENTH) {
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  	"Found invalid iterate_macro statement syntax.",peek(tok).line,"(",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume (
    if(peek(tok).type != token::KEYWORD) {
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid macro header in iterate_macro statement syntax.",peek(tok).line,"valid <macro_header>",peek(tok).val, peek(tok).file));
    }
    Token macro = consume(tok);
	if(!is_defined_macro(macro.val)) {
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Called macro in iterate_macro statement has not been defined.",peek(tok).line,"",peek(tok).val, peek(tok).file));
	}
    if(peek(tok).type != token::CLOSED_PARENTH) {
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid iterate_macro statement syntax.",peek(tok).line,")",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume )
    if(peek(tok).type != token::ASSIGN) {
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid iterate_macro statement syntax.",peek(tok).line,":=",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume :=
    auto from_stmt = parse_binary_expr(tok);
    if(from_stmt.is_error())
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(from_stmt.get_error());
    SimpleInterpreter interpreter;
    auto from_interpreted = interpreter.evaluate_int_expression(from_stmt.unwrap());
    if(from_interpreted.is_error())
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(from_interpreted.get_error());
    int from = from_interpreted.unwrap();
    if(not(peek(tok).type == token::TO or peek(tok).type == token::DOWNTO)) {
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid iterate_macro statement syntax.",peek(tok).line,"to/downto",peek(tok).val, peek(tok).file));
    }
	bool is_downto = peek(tok).type == token::DOWNTO;
    consume(tok); // consume "to"
    auto to_stmt = parse_binary_expr(tok);
    if(to_stmt.is_error())
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(to_stmt.get_error());
    auto to_interpreted = interpreter.evaluate_int_expression(to_stmt.unwrap());
    if(to_interpreted.is_error())
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(to_interpreted.get_error());
    int to = to_interpreted.unwrap();
    if(peek(tok).type != token::LINEBRK) {
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid iterate_macro statement syntax. Missing linebreak after iterate_macro statement.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
    }
    consume(tok); //consume linebreak
    remove_tokens(tok, begin, m_pos);

	auto macro_header_result = get_macro_definition(macro, 1);
	if(macro_header_result.is_error())
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(macro_header_result.get_error());
    std::vector<std::unique_ptr<NodeMacroHeader>> iterate_macros={};
    iterate_macros.reserve(std::abs(from-to));
	std::unique_ptr<NodeMacroHeader> macro_header = std::move(macro_header_result.unwrap()->header);
	macro_header->token_pos = begin;
	std::vector<std::vector<Token>> args(1, std::vector<Token>(1));

    if (is_downto) {
        for (int i = to; i <= from; i++) {
            args[0][0] = {Token{INT, std::to_string(i), macro.line, macro.file}};
            iterate_macros.push_back(std::make_unique<NodeMacroHeader>(macro_header->name, args, macro_header->token_pos, macro_header->tok));
        }
    } else {
        for (int i = to; i >= from; i--) {
            args[0][0] = {Token{INT, std::to_string(i), macro.line, macro.file}};
            iterate_macros.push_back(std::make_unique<NodeMacroHeader>(macro_header->name, args, macro_header->token_pos, macro_header->tok));
        }
    }

    return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(std::move(iterate_macros));
}

Result<std::vector<std::unique_ptr<NodeMacroHeader>>> PreprocessorMacros::parse_literate_macro(std::vector<Token>& tok) {
	size_t begin = m_pos;
	consume(tok); // consume literate_macro
	if(peek(tok).type != token::OPEN_PARENTH) {
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid literate_macro statement syntax.",peek(tok).line,"(",peek(tok).val, peek(tok).file));
	}
	consume(tok); //consume (
	if(peek(tok).type != token::KEYWORD) {
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid macro header in literate_macro statement syntax.",peek(tok).line,"valid <macro_header>",peek(tok).val, peek(tok).file));
	}
	Token macro = consume(tok);
	if(!is_defined_macro(macro.val)) {
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Called macro in literate_macro statement has not been defined.",peek(tok).line,"",peek(tok).val, peek(tok).file));
	}
	if(peek(tok).type != token::CLOSED_PARENTH) {
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid literate_macro statement syntax.",peek(tok).line,")",peek(tok).val, peek(tok).file));
	}
	consume(tok); //consume )
	if(peek(tok).type != token::ON) {
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid literate_macro statement syntax.",peek(tok).line,"on",peek(tok).val, peek(tok).file));
	}
    consume(tok); //consume on
	std::vector<Token> literate_params = {};
	do {
		if(peek(tok).type == COMMA) consume(tok);
		if(peek(tok).type != KEYWORD)
            return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
          "Found invalid literate_macro statement syntax. Missing <keyword> in keyword enumeration ",peek(tok).line,"<keyword>",peek(tok).val, peek(tok).file));
        literate_params.push_back(consume(tok));
	} while(peek(tok).type == token::COMMA);

	if(peek(tok).type != token::LINEBRK) {
		return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(CompileError(ErrorType::SyntaxError,
	  "Found invalid literate_macro statement syntax. Missing linebreak after literate_macro statement.",peek(tok).line,"linebreak",peek(tok).val, peek(tok).file));
	}
	consume(tok); //consume linebreak
	remove_tokens(tok, begin, m_pos);

    auto macro_header_result = get_macro_definition(macro, 1);
    if(macro_header_result.is_error())
        return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(macro_header_result.get_error());
    std::vector<std::unique_ptr<NodeMacroHeader>> literate_macros={};

    std::unique_ptr<NodeMacroHeader> macro_header = std::move(macro_header_result.unwrap()->header);
    macro_header->token_pos = begin;
    std::vector<std::vector<Token>> args(1, std::vector<Token>(1));

    for (auto it = literate_params.rbegin(); it != literate_params.rend(); ++it) {
        // Verwenden Sie *it, um auf das aktuelle Element zuzugreifen
        args[0][0] = {*it};
        auto new_macro_header = std::make_unique<NodeMacroHeader>(macro_header->name,args,macro_header->token_pos,macro_header->tok);
        literate_macros.push_back(std::move(new_macro_header));
    }

    return Result<std::vector<std::unique_ptr<NodeMacroHeader>>>(std::move(literate_macros));
}


Result<std::unique_ptr<NodeMacroDefinition>> PreprocessorMacros::get_macro_definition(Token& macro_name, int num_args) {
	for(auto &macro_def : m_macro_definitions) {
		if(macro_name.val == macro_def->header->name and macro_def->header->args.size() == num_args) {
			auto m_header = std::make_unique<NodeMacroHeader>(macro_def->header->name,macro_def->header->args,macro_def->header->token_pos,macro_def->header->tok);
			auto m_def = std::make_unique<NodeMacroDefinition>(std::move(m_header),macro_def->body,macro_def->has_recursive_calls, macro_def->tok);
			return Result<std::unique_ptr<NodeMacroDefinition>>(std::move(m_def));
		}
	}
	return Result<std::unique_ptr<NodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
	"Found incorrect number of parameters in macro call.",macro_name.line,"valid number of parameters",std::to_string(num_args),macro_name.file));
}
