//
// Created by Mathias Vatter on 10.11.23.
//

#include "PreprocessorParser.h"

Result<std::unique_ptr<PreNodeProgram>> PreprocessorParser::parse_program(PreNodeAST *parent) {
    auto program = std::make_unique<PreNodeChunk>(Token(), parent);
    std::vector<std::unique_ptr<PreNodeDefineStatement>> define_statements;
    std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions;
    auto node_program = std::make_unique<PreNodeProgram>(std::move(program), std::move(define_statements), std::move(macro_definitions), Token(), nullptr);
    node_program->def_provider = m_definition_provider;
    m_program = node_program.get();
    m_pos = 0;
    // get definitions first
    while(peek().type != token::END_TOKEN) {
        if (is_define_definition()) {
            consume(); // consume define
            int num_args = get_num_params_in_definition();
            auto name = consume().val;
            m_define_strings.insert({{name, num_args}, name});
        } else if (is_macro_definition()) {
            consume(); // consume macro
            // get number of arguments to not get define and macro confused
            int num_args = get_num_params_in_definition();
            auto name = consume().val;
            m_macro_iterate_strings.insert(name);
            m_macro_strings.insert({{name, num_args}, name});
        } else consume();
    }
    // get construct calls
    m_pos = 0;
    while(peek().type != token::END_TOKEN) {
        if (is_define_definition()) {
            auto result_define = parse_define_definition(parent);
            if (result_define.is_error())
                return Result<std::unique_ptr<PreNodeProgram>>(result_define.get_error());
            m_program->define_statements.push_back(std::move(result_define.unwrap()));
//        } else if (is_macro_definition()) {
//            auto result_macro_def = parse_macro_definition(parent);
//            if (result_macro_def.is_error())
//                return Result<std::unique_ptr<PreNodeProgram>>(result_macro_def.get_error());
//            m_program->macro_definitions.push_back(std::move(result_macro_def.unwrap()));
        } else {
            auto token_result = parse_token(parent);
            if (token_result.is_error())
                return Result<std::unique_ptr<PreNodeProgram>>(token_result.get_error());
            m_program->program->add_chunk(std::move(token_result.unwrap()));
        }
    }
    auto node_end_token = std::make_unique<PreNodeOther>(std::move(consume()), parent);
//    node_program->define_statements = std::move(m_define_definitions);
    node_program->program->add_chunk(std::move(node_end_token));
    return Result<std::unique_ptr<PreNodeProgram>>(std::move(node_program));
}


bool PreprocessorParser::is_define_definition() {
    if(m_pos >0) {
        return m_pos > 0 and peek().type == token::DEFINE and peek(-1).type == token::LINEBRK;
    }
    return peek().type == token::DEFINE;
}

bool PreprocessorParser::is_macro_definition() {
    if(m_pos >0) {
        return m_pos > 0 and peek().type == token::MACRO and peek(-1).type == token::LINEBRK;
    }
    return peek().type == token::MACRO;
}

bool PreprocessorParser::is_define_call(const Token &tok) {
    bool syntax = false;
    if(m_pos >0) {
        syntax = (peek().type == token::KEYWORD and m_pos > 0 and (peek(-1).type != token::MACRO or peek(-1).type != token::FUNCTION));
    }
    syntax = peek().type == token::KEYWORD;

    if(syntax) {
        //search in m_define_strings
        int num_args = get_num_params_in_definition();
        auto it = m_define_strings.find({tok.val, num_args});
        syntax &= it != m_define_strings.end();
    }
    return syntax;
}

bool PreprocessorParser::is_macro_call(const Token &tok) {
	if(peek().type != token::KEYWORD) return false;

	bool is_macro_call = peek().type == token::KEYWORD;
	bool is_iterator_macro_call = peek().type == token::KEYWORD;

	if(m_pos<2) {
		// iterator macro call cannot be at first position in file
		is_iterator_macro_call = false;
		// macro call has to have linebreak or open parenth after it
		is_macro_call &= peek(1).type == token::OPEN_PARENTH or peek(1).type == token::LINEBRK;
	} else {
	    is_iterator_macro_call &= peek(-2).type == token::ITERATE_MACRO || peek(-2).type == token::LITERATE_MACRO;
		// iterator macro call has to have open parenth beforehand
		is_iterator_macro_call &= peek(-1).type == token::OPEN_PARENTH and (m_parsing_iterator_macro || m_parsing_literate_macro) and (peek(1).type == token::OPEN_PARENTH or peek(1).type == token::CLOSED_PARENTH);
		// macro call has to have linebreak before and either linebreak or open parenth after it
		is_macro_call &= peek(-1).type == token::LINEBRK and (peek(1).type == token::OPEN_PARENTH or peek(1).type == token::LINEBRK);
	}

    int num_hashes = StringUtils::count_char(tok.val, '#');
	if(is_iterator_macro_call) {
		is_iterator_macro_call &= m_macro_iterate_strings.contains(tok.val) or (num_hashes > 0 and num_hashes % 2 == 0);
		return is_iterator_macro_call;
	}
	if(is_macro_call) {
		int num_args = get_num_params_in_definition();
		//search in m_define_strings
		is_macro_call &= m_macro_strings.contains({tok.val, num_args}) or (num_hashes > 0 and num_hashes % 2 == 0);
		return is_macro_call;
	}
	return false;
}

int PreprocessorParser::get_num_params_in_definition() {
    size_t begin = m_pos;
    consume(); // consume name
    _skip_linebreaks();
    int num_params = 0;

    if(peek().type == token::OPEN_PARENTH) {
        consume(); // consume (
        _skip_linebreaks();
        if (peek().type != token::CLOSED_PARENTH) {
            int parenth_depth = 1; // Start with 1 because we've already consumed the first OPEN_PARENTH
            while (parenth_depth > 0) {
                if (peek().type == token::OPEN_PARENTH || peek().type == token::OPEN_BRACKET) {
                    parenth_depth++;
                } else if (peek().type == token::CLOSED_PARENTH || peek().type == token::CLOSED_BRACKET) {
                    _skip_linebreaks();
                    parenth_depth--;
                } else if (peek().type == token::LINEBRK) {
                    _skip_linebreaks();
                    continue;
                } else if (peek().type == token::END_TOKEN) {
                    auto error = CompileError(ErrorType::PreprocessorError,
                "",")", peek());
                    error.add_message("Unexpected end of file. Missing closing parenthesis.");
                    error.exit();
                }
                if (peek().type == token::COMMA && parenth_depth == 1) {
                    num_params++;
                    consume(); // consume COMMA
                    _skip_linebreaks();
                } else if (parenth_depth > 0) {
                    consume();
                }
            }
            if(num_params>0) {
                num_params += 2;
            } else {
                num_params = 1;
            }
        }
    }

    m_pos = begin;
    return num_params;
}


Result<std::unique_ptr<PreNodeAST>> PreprocessorParser::parse_token(PreNodeAST* parent) {
    std::unique_ptr<PreNodeAST> stmt;
    auto start_token = peek();
    auto node_statement = std::make_unique<PreNodeStatement>(start_token, parent);
	if (is_macro_definition()) {
		auto result_macro_def = parse_macro_definition(node_statement.get());
		if (result_macro_def.is_error())
			return Result<std::unique_ptr<PreNodeAST>>(result_macro_def.get_error());
		node_statement->statement = std::move(result_macro_def.unwrap());
		stmt = std::move(node_statement);
	} else if(is_define_call(peek())) {
        auto result_define_call = parse_define_call(node_statement.get());
        if (result_define_call.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_define_call.get_error());
        node_statement->statement = std::move(result_define_call.unwrap());
        stmt = std::move(node_statement);
    } else if(is_macro_call(peek())) {
        auto result_macro_call = parse_macro_call(node_statement.get());
        if (result_macro_call.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_macro_call.get_error());
        node_statement->statement = std::move(result_macro_call.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == token::ITERATE_MACRO) {
        auto result_iterate_macro = parse_iterate_macro(node_statement.get());
        if(result_iterate_macro.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_iterate_macro.get_error());
        node_statement->statement = std::move(result_iterate_macro.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == token::LITERATE_MACRO) {
        auto result_literate_macro = parse_literate_macro(node_statement.get());
        if(result_literate_macro.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_literate_macro.get_error());
        node_statement->statement = std::move(result_literate_macro.unwrap());
        stmt = std::move(node_statement);
    } else if (peek().type == token::SET_CONDITION) {
        auto result_condition_def = parse_set_condition(node_statement.get());
        if (result_condition_def.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_condition_def.get_error());
        node_statement->statement = std::move(result_condition_def.unwrap());
        stmt = std::move(node_statement);
    } else if (peek().type == token::RESET_CONDITION) {
        auto result_condition_def = parse_reset_condition(node_statement.get());
        if (result_condition_def.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_condition_def.get_error());
        node_statement->statement = std::move(result_condition_def.unwrap());
        stmt = std::move(node_statement);
    } else if (peek().type == token::IMPORT) {
        auto result_import = parse_import(node_statement.get());
        if (result_import.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_import.get_error());
        node_statement->statement = std::move(result_import.unwrap());
        stmt = std::move(node_statement);
    } else if (peek().type == token::USE_CODE_IF || peek().type == token::USE_CODE_IF_NOT) {
        auto result_use_code_if = parse_use_code_if(node_statement.get());
        if (result_use_code_if.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_use_code_if.get_error());
        node_statement->statement = std::move(result_use_code_if.unwrap());
        stmt = std::move(node_statement);
    } else if (peek().type == token::KEYWORD and peek().val == "import_nckp") {
        auto result_import_nckp = parse_import_nckp(node_statement.get());
        if (result_import_nckp.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_import_nckp.get_error());
        node_statement->statement = std::move(result_import_nckp.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == token::KEYWORD or peek().type == token::STRING or peek().type == token::DEFAULT or peek().type == token::RETURN) {
        auto result_keyword = parse_keyword(node_statement.get());
        if (result_keyword.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_keyword.get_error());
        node_statement->statement = std::move(result_keyword.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == token::INT) {
        auto result_int = parse_int(node_statement.get());
        if (result_int.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_int.get_error());
        node_statement->statement = std::move(result_int.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == token::FLOAT or peek().type == token::BINARY or peek().type == token::HEXADECIMAL) {
        auto result_number = parse_number(node_statement.get());
        if (result_number.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_number.get_error());
        node_statement->statement = std::move(result_number.unwrap());
        stmt = std::move(node_statement);
    } else if (peek().type == token::START_INC) {
        auto result_inc = parse_incrementer(node_statement.get());
        if (result_inc.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_inc.get_error());
        node_statement->statement = std::move(result_inc.unwrap());
        stmt = std::move(node_statement);
    } else if (peek().type == token::PRAGMA) {
        auto result_pragma = parse_pragma(node_statement.get());
        if (result_pragma.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_pragma.get_error());
        node_statement->statement = std::move(result_pragma.unwrap());
        stmt = std::move(node_statement);
    } else {
        auto result_other = parse_other(parent);
        if(result_other.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_other.get_error());
        stmt = std::move(result_other.unwrap());
    }
    stmt->set_range(start_token, peek(-1));
    return Result<std::unique_ptr<PreNodeAST>>(std::move(stmt));
}

Result<std::unique_ptr<PreNodeNumber>> PreprocessorParser::parse_number(PreNodeAST *parent) {
    auto number_token = peek();
    auto node_number = std::make_unique<PreNodeNumber>(consume(), parent);
    node_number->set_range(number_token);
    return Result<std::unique_ptr<PreNodeNumber>>(std::move(node_number));
}

Result<std::unique_ptr<PreNodeAST>> PreprocessorParser::parse_int(PreNodeAST *parent) {
    auto token = consume();
    auto value = token.val;
    try {
        long long val = std::stoll(value, nullptr, 10);
        auto node = std::make_unique<PreNodeInt>(static_cast<int32_t>(val & 0xFFFFFFFF), token, parent);
        node->set_range(token);
        return Result<std::unique_ptr<PreNodeAST>>(std::move(node));
    } catch (const std::exception& e) {
        auto expected = std::string(1, "valid int base "[10]);
        return Result<std::unique_ptr<PreNodeAST>>(CompileError(ErrorType::PreprocessorError,
        "Invalid integer format.", token.line, expected, value, token.file));
    }
}

Result<std::unique_ptr<PreNodeKeyword>> PreprocessorParser::parse_keyword(PreNodeAST *parent) {
    auto token = peek();
    auto node_keyword = std::make_unique<PreNodeKeyword>(consume(), parent);
    node_keyword->set_range(token);
    return Result<std::unique_ptr<PreNodeKeyword>>(std::move(node_keyword));
}

Result<std::unique_ptr<PreNodeOther>> PreprocessorParser::parse_other(PreNodeAST *parent) {
    auto token = peek();
    auto node_other = std::make_unique<PreNodeOther>(consume(), parent);
    node_other->set_range(token);
    return Result<std::unique_ptr<PreNodeOther>>(std::move(node_other));
}

Result<std::unique_ptr<PreNodeList>> PreprocessorParser::parse_list(PreNodeAST *parent) {
    std::vector<std::unique_ptr<PreNodeChunk>> params_list = {};
    auto node_list = std::make_unique<PreNodeList>(std::move(params_list), peek(), parent);
    if (peek().type != token::OPEN_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, "", "(", peek());
        error.set_message("Missing open parenthesis in <list> syntax.");
        return Result<std::unique_ptr<PreNodeList>>(error);
    }
    auto start_token = consume(); // consume (
    _skip_linebreaks();
    if (peek().type != token::CLOSED_PARENTH) {
        int parenth_depth = 1; // Start with 1 because we've already consumed the first OPEN_PARENTH
        auto node_chunk = std::make_unique<PreNodeChunk>(peek(), node_list.get());
        while (parenth_depth > 0) {
            // Swallow any number of line breaks while inside parentheses
            if (peek().type == token::LINEBRK) {
                _skip_linebreaks();
                continue;
            }
            if (peek().type == token::OPEN_PARENTH or peek().type == token::OPEN_BRACKET) {
                parenth_depth++;
            } else if (peek().type == token::CLOSED_PARENTH or peek().type == token::CLOSED_BRACKET) {
                parenth_depth--;
            } else if (peek().type == token::END_TOKEN) {
                return Result<std::unique_ptr<PreNodeList>>(CompileError(ErrorType::PreprocessorError,
            "Unexpected end of file. Missing closing parenthesis.",")", peek()));
    //         } else if (peek().type == token::LINEBRK) {
				// return Result<std::unique_ptr<PreNodeList>>(CompileError(ErrorType::SyntaxError,
				//  "Unexpected linebreak. Missing closing parenthesis.",")", peek()));
			}
            if (peek().type == token::COMMA && parenth_depth == 1) {
                node_list->add_element(std::move(node_chunk));
                node_chunk = std::make_unique<PreNodeChunk>(peek(), node_list.get());
                consume(); // consume COMMA
                _skip_linebreaks();
            } else if(parenth_depth > 0) {
                auto result_token = parse_token(node_chunk.get());
                if(result_token.is_error())
                    return Result<std::unique_ptr<PreNodeList>>(result_token.get_error());
                node_chunk->add_chunk(std::move(result_token.unwrap()));
            }
        }
        if (node_chunk != nullptr) {
            node_list->add_element(std::move(node_chunk));
        }
    }
    _skip_linebreaks();
    auto end_token = consume(); //consume )
    node_list->set_range(start_token, end_token);
    return Result<std::unique_ptr<PreNodeList>>(std::move(node_list));
}

Result<std::unique_ptr<PreNodeDefineHeader>> PreprocessorParser::parse_define_header(PreNodeAST *parent) {
    auto start_token = peek();
    auto node_define_header = std::make_unique<PreNodeDefineHeader>(start_token, parent);
    auto define_name = parse_keyword(node_define_header.get());
    if(define_name.is_error())
        return Result<std::unique_ptr<PreNodeDefineHeader>>(define_name.get_error());
    node_define_header->name = std::move(define_name.unwrap());
    auto define_args = std::make_unique<PreNodeList>(peek(), node_define_header.get());
    if(peek().type == token::OPEN_PARENTH) {
        auto define_args_result = parse_list(node_define_header.get());
        if (define_args_result.is_error())
            return Result<std::unique_ptr<PreNodeDefineHeader>>(define_args_result.get_error());
        define_args = std::move(define_args_result.unwrap());
    }
    node_define_header->set_args(std::move(define_args));
    node_define_header->set_range(start_token, peek(-1));
    return Result<std::unique_ptr<PreNodeDefineHeader>>(std::move(node_define_header));
}

Result<std::unique_ptr<PreNodeDefineStatement>> PreprocessorParser::parse_define_definition(PreNodeAST *parent) {
    auto start_token = consume(); //consume define
    auto define_statement = std::make_unique<PreNodeDefineStatement>(start_token, parent);
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Missing define name.",peek().line,"<keyword>",peek().val, peek().file));
    }
    auto define_header_result = parse_define_header(define_statement.get());
    if(define_header_result.is_error())
        return Result<std::unique_ptr<PreNodeDefineStatement>>(define_header_result.get_error());
    auto header = define_header_result.unwrap()->get_name();
    if(peek().type != token::ASSIGN)
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Found invalid Define Statement Syntax. Missing <assign> symbol.", peek().line, ":=", peek().val, peek().file));
    consume(); //consume :=

    auto node_chunk = std::make_unique<PreNodeChunk>(peek(), parent);
    while(peek().type != token::LINEBRK) {
        if (peek().type == token::END_TOKEN)
            return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
         "Unexpected end of m_tokens. Missing assignment of define statement.",peek().line, "", peek().val,peek().file));

        if(peek().type == token::KEYWORD and define_header_result.unwrap()->name->tok.val == peek().val) {
            return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	    "A define constant cannot define itself.",peek().line,"","", peek().file));
		}
		auto result_token = parse_token(node_chunk.get());
        if(result_token.is_error()) {
            return Result<std::unique_ptr<PreNodeDefineStatement>>(result_token.get_error());
		}
        node_chunk->add_chunk(std::move(result_token.unwrap()));
    }
    if(node_chunk->chunk.empty()) {
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Found empty define statement assignment.",peek().line, "", peek().val,peek().file));
    }
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Missing necessary linebreak after define statement.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); //consume linebreak
    define_statement->header = std::move(define_header_result.unwrap());
    define_statement->body = std::move(node_chunk);
    define_statement->set_range(start_token, peek(-1));
    return Result<std::unique_ptr<PreNodeDefineStatement>>(std::move(define_statement));
}

Result<std::unique_ptr<PreNodeDefineCall>> PreprocessorParser::parse_define_call(PreNodeAST *parent) {
    auto start_token = peek();
    auto node_define_call = std::make_unique<PreNodeDefineCall>(start_token, parent);
    auto define_header_result = parse_define_header(node_define_call.get());
    if(define_header_result.is_error())
        return Result<std::unique_ptr<PreNodeDefineCall>>(define_header_result.get_error());
    node_define_call->define = std::move(define_header_result.unwrap());
    node_define_call->set_range(start_token, peek(-1));
    return Result<std::unique_ptr<PreNodeDefineCall>>(std::move(node_define_call));
}

Result<std::unique_ptr<PreNodeMacroHeader>> PreprocessorParser::parse_macro_header(PreNodeAST* parent) {
    auto start_token = peek();
    auto node_macro_header = std::make_unique<PreNodeMacroHeader>( start_token, parent);
    auto macro_name = parse_keyword(node_macro_header.get());
    if(macro_name.is_error())
        return Result<std::unique_ptr<PreNodeMacroHeader>>(macro_name.get_error());
    node_macro_header->name = std::move(macro_name.unwrap());
    auto macro_args = std::make_unique<PreNodeList>(peek(), node_macro_header.get());

    if (peek().type == token::OPEN_PARENTH) {
		node_macro_header->has_parenth = true;
        auto macro_args_result = parse_list(node_macro_header.get());
        if (macro_args_result.is_error())
            return Result<std::unique_ptr<PreNodeMacroHeader>>(macro_args_result.get_error());
        macro_args = std::move(macro_args_result.unwrap());
    }
    node_macro_header->set_args(std::move(macro_args));
    node_macro_header->set_range(start_token, peek(-1));
    return Result<std::unique_ptr<PreNodeMacroHeader>>(std::move(node_macro_header));
}

Result<std::unique_ptr<PreNodeMacroCall>> PreprocessorParser::parse_macro_call(PreNodeAST* parent) {
    auto start_token = peek();
    auto node_macro_call = std::make_unique<PreNodeMacroCall>(start_token, parent);
    auto macro_stmt = parse_macro_header(node_macro_call.get());
    if(macro_stmt.is_error()){
        return Result<std::unique_ptr<PreNodeMacroCall>>(macro_stmt.get_error());
    }
	node_macro_call->is_iterate_macro = m_parsing_iterator_macro;
    node_macro_call->macro = std::move(macro_stmt.unwrap());
    node_macro_call->set_range(start_token, peek(-1));
    return Result<std::unique_ptr<PreNodeMacroCall>>(std::move(node_macro_call));
}

Result<std::unique_ptr<PreNodeMacroDefinition>> PreprocessorParser::parse_macro_definition(PreNodeAST* parent) {
    auto start_token = consume(); // consume macro
    auto node_macro_definition = std::make_unique<PreNodeMacroDefinition>(start_token, parent);
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<PreNodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
    "Missing macro name.",peek().line,"keyword",peek().val, peek().file));
    }
    auto header = parse_macro_header(node_macro_definition.get());
    if (header.is_error()) {
        return Result<std::unique_ptr<PreNodeMacroDefinition>>(header.get_error());
    }
    node_macro_definition->header = std::move(header.unwrap());
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<PreNodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
     "Missing necessary linebreak after macro header.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak
    auto node_chunk = std::make_unique<PreNodeChunk>(peek(), node_macro_definition.get());
    while (peek().type != token::END_MACRO) {
        if(peek().type == token::END_MACRO) break;

        if (peek().type == token::MACRO) {
            return Result<std::unique_ptr<PreNodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
        "Nested macros are not allowed. Maybe you forgot an 'end macro' along the line?",peek().line, "", peek().val,peek().file));
        } else if (is_define_definition()) {
            auto result_define = parse_define_definition(node_chunk.get());
            if (result_define.is_error())
                return Result<std::unique_ptr<PreNodeMacroDefinition>>(result_define.get_error());
            m_program->define_statements.push_back(std::move(result_define.unwrap()));
        } else {
            auto result_token = parse_token(node_chunk.get());
            if(result_token.is_error())
                return Result<std::unique_ptr<PreNodeMacroDefinition>>(result_token.get_error());
            node_chunk->chunk.push_back(std::move(result_token.unwrap()));
        }
    }
    auto end_token = consume(); // consume end macro
    node_macro_definition->body = std::move(node_chunk);
    node_macro_definition->set_range(start_token, end_token);
    return Result<std::unique_ptr<PreNodeMacroDefinition>>(std::move(node_macro_definition));
}

Result<std::unique_ptr<PreNodeIterateMacro>> PreprocessorParser::parse_iterate_macro(PreNodeAST* parent) {
    auto start_token = consume(); // consume iterate_macro
    auto node_iterate_macro = std::make_unique<PreNodeIterateMacro>(start_token, parent);
	if(m_parsing_iterator_macro || m_parsing_literate_macro) {
		CompileError(ErrorType::SyntaxError,"Found nested macro iteration.", peek().line, "", "", peek().file).exit();
	}
    m_parsing_iterator_macro = true;
    if(peek().type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<PreNodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
      "Found invalid <iterate_macro> statement syntax.",peek().line,"(",peek().val, peek().file));
    }
    auto node_statement = parse_list(node_iterate_macro.get());
    if(node_statement.is_error())
        return Result<std::unique_ptr<PreNodeIterateMacro>>(node_statement.get_error());

    if(peek().type != token::ASSIGN) {
        return Result<std::unique_ptr<PreNodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
      "Found invalid <iterate_macro> statement syntax.", peek().line,":=", peek().val, peek().file));
    }
    consume(); // consume :=

    auto node_iterator_start = std::make_unique<PreNodeChunk>(peek(), node_iterate_macro.get());
    while(peek().type != token::LINEBRK) {
        if(peek().type == token::TO or peek().type == token::DOWNTO) break;
        auto result_token = parse_token(node_iterator_start.get());
        if(result_token.is_error())
            return Result<std::unique_ptr<PreNodeIterateMacro>>(result_token.get_error());
        node_iterator_start->add_chunk(std::move(result_token.unwrap()));
    }

    if(not(peek().type == token::TO or peek().type == token::DOWNTO)) {
        return Result<std::unique_ptr<PreNodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
      "Found invalid <iterate_macro> statement syntax.", peek().line,"<to>/<downto>", peek().val, peek().file));
    }
    Token to = consume(); // consume downto/to

    auto node_iterator_end = std::make_unique<PreNodeChunk>(peek(), node_iterate_macro.get());
    while(peek().type != token::LINEBRK) {
        if(peek().type == token::STEP) break;
        auto result_token = parse_token(node_iterator_end.get());
        if(result_token.is_error())
            return Result<std::unique_ptr<PreNodeIterateMacro>>(result_token.get_error());
        node_iterator_end->add_chunk(std::move(result_token.unwrap()));
    }

    auto step = std::make_unique<PreNodeChunk>(peek(), node_iterate_macro.get());
    auto step_statement = std::make_unique<PreNodeStatement>(peek(), step.get());
//    Token toki = Token(INT, "1", 0, "");
    auto int_tok = peek(); int_tok.set_type(token::INT); int_tok.set_val("1");
    auto node_int = std::make_unique<PreNodeInt>(1,  int_tok, step_statement.get());
    step_statement->statement = std::move(node_int);
    step->add_chunk(std::move(step_statement));
    if(peek().type == token::STEP) {
        consume(); // consume step
        auto node_step = std::make_unique<PreNodeChunk>(peek(), parent);
        while(peek().type != token::LINEBRK) {
            auto result_token = parse_token(node_step.get());
            if(result_token.is_error())
                return Result<std::unique_ptr<PreNodeIterateMacro>>(result_token.get_error());
            node_step->add_chunk(std::move(result_token.unwrap()));
        }
        step = std::move(node_step);
    }
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<PreNodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
    "Missing necessary linebreak after <iterate_macro> statement.",peek().line,"linebreak",peek().val, peek().file));
    }

    consume(); // consume linebreak
	_skip_linebreaks();
    node_iterate_macro->macro_call = std::move(node_statement.unwrap());
    node_iterate_macro->iterator_start = std::move(node_iterator_start);
    node_iterate_macro->iterator_end = std::move(node_iterator_end);
    node_iterate_macro -> to = to;
    node_iterate_macro->step = std::move(step);
    m_parsing_iterator_macro = false;
    node_iterate_macro->set_range(start_token, peek(-1));
    return Result<std::unique_ptr<PreNodeIterateMacro>>(std::move(node_iterate_macro));
}

Result<std::unique_ptr<PreNodeLiterateMacro>> PreprocessorParser::parse_literate_macro(PreNodeAST* parent) {
    auto start_token = consume(); // consume literate_macro
    auto node_literate_macro = std::make_unique<PreNodeLiterateMacro>(start_token, parent);
	if(m_parsing_iterator_macro || m_parsing_literate_macro) {
		CompileError(ErrorType::SyntaxError,"Found nested macro iteration.", peek().line, "", "", peek().file).exit();
	}
	m_parsing_literate_macro = true;
    if(peek().type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<PreNodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
       "Found invalid <literate_macro> statement syntax.",peek().line,"(",peek().val, peek().file));
    }
    auto node_statement = parse_list(node_literate_macro.get());
    if(node_statement.is_error())
        return Result<std::unique_ptr<PreNodeLiterateMacro>>(node_statement.get_error());

    if(peek().type != token::ON) {
        return Result<std::unique_ptr<PreNodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
       "Found invalid <literate_macro> statement syntax.", peek().line,"on", peek().val, peek().file));
    }
    consume(); // consume on
    auto node_chunk = std::make_unique<PreNodeChunk>(peek(), node_literate_macro.get());
    while(peek().type != token::LINEBRK) {
        if(peek().type == token::KEYWORD or peek().type == token::STRING) {
            auto result_token = parse_token(node_chunk.get());
            if (result_token.is_error())
                return Result<std::unique_ptr<PreNodeLiterateMacro>>(result_token.get_error());
            node_chunk->chunk.push_back(std::move(result_token.unwrap()));
        } else {
            return Result<std::unique_ptr<PreNodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
          "Found invalid <literate_macro> statement syntax. Can only literate on <keywords>.",peek().line,"<keyword>",peek().val, peek().file));
        }
        if(peek().type == token::COMMA)
            consume();
    }

    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<PreNodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
     "Missing necessary linebreak after <literate_macro> statement.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak

    node_literate_macro->macro_call = std::move(node_statement.unwrap());
    node_literate_macro->literate_tokens = std::move(node_chunk);
    node_literate_macro->set_range(start_token, peek(-1));
	m_parsing_literate_macro = false;
    return Result<std::unique_ptr<PreNodeLiterateMacro>>(std::move(node_literate_macro));
}

Result<std::unique_ptr<PreNodeIncrementer>> PreprocessorParser::parse_incrementer(PreNodeAST *parent) {
    Token start_inc = consume(); // consume START_INC
    auto node_incrementer = std::make_unique<PreNodeIncrementer>(start_inc, parent);
    constexpr auto error_msg = "Found invalid <START_INC> statement syntax. Correct syntax is: <START_INC> (<name>, <start>, <step>)\\n<code to increment>\\n<END_INC>";
    if(peek().type != token::OPEN_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg, "(", peek());
        error.set_message("Missing open parenthesis in <START_INC> statement syntax.");
        return Result<std::unique_ptr<PreNodeIncrementer>>(error);
    }
    auto node_list = parse_list(node_incrementer.get());
    if(node_list.is_error())
        return Result<std::unique_ptr<PreNodeIncrementer>>(node_list.get_error());
    auto list = std::move(node_list.unwrap());
    if(list->params.size() != 3) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg, "<name>, <start>, <step>", peek());
        error.set_message("Expected exactly 3 parameters in <START_INC> statement syntax: <name>, <start>, <step>.");
        error.m_got = list->get_string();
        return Result<std::unique_ptr<PreNodeIncrementer>>(error);
    }
    // check if <name> argument only consists of one token
    if(list->params[0]->num_chunks() != 1) {
        auto error = CompileError(ErrorType::PreprocessorError,error_msg, "<name>", peek());
        error.m_got = list->params[0]->get_string();
        error.add_message("Found too many tokens in <START_INC> <name> argument.");
        return Result<std::unique_ptr<PreNodeIncrementer>>(error);
    }
    if (peek().type != token::LINEBRK) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg, "linebreak", peek());
        error.set_message("Missing linebreak after <START_INC> statement.");
        return Result<std::unique_ptr<PreNodeIncrementer>>(error);
    }
    consume(); // consume linebreak
    while (peek().type != token::END_INC) {
        auto node_chunk = std::make_unique<PreNodeChunk>(peek(), node_incrementer.get());
        while(peek().type != token::LINEBRK) {
            if(peek().type == token::END_INC) break;
            if(peek().type == token::END_TOKEN) {
                auto error = CompileError(ErrorType::PreprocessorError, error_msg, "end", peek());
                error.set_message("Missing <END_INC> statement. Reached the end of the file while parsing an incrementer.");
                return Result<std::unique_ptr<PreNodeIncrementer>>(error);
            }
            if (is_define_definition()) {
                auto result_define = parse_define_definition(parent);
                if (result_define.is_error())
                    return Result<std::unique_ptr<PreNodeIncrementer>>(result_define.get_error());
                m_program->define_statements.push_back(std::move(result_define.unwrap()));
            } else {
                auto token_result = parse_token(parent);
                if (token_result.is_error())
                    return Result<std::unique_ptr<PreNodeIncrementer>>(token_result.get_error());
                node_chunk->chunk.push_back(std::move(token_result.unwrap()));
            }

        }
        if(peek().type == token::LINEBRK) {
            auto node_other = std::make_unique<PreNodeOther>(consume(), parent);
            node_chunk->chunk.push_back(std::move(node_other));
        }
        if(!node_chunk->chunk.empty())
            node_incrementer->body.push_back(std::move(node_chunk));
    }
    auto end_inc = consume(); // consume END_INC

    if (peek().type != token::LINEBRK) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg, "linebreak", peek());
        error.set_message("Missing linebreak after <START_INC> statement.");
        return Result<std::unique_ptr<PreNodeIncrementer>>(error);
    }
    consume(); // consume linebreak

    node_incrementer->set_counter(std::move(list->params[0]));
    node_incrementer->set_iterator_start(std::move(list->params[1]));
    node_incrementer->set_iterator_step(std::move(list->params[2]));
    node_incrementer->set_range(start_inc, end_inc);
    return Result<std::unique_ptr<PreNodeIncrementer>>(std::move(node_incrementer));
}

Result<std::unique_ptr<PreNodeImport>> PreprocessorParser::parse_import(PreNodeAST *parent) {
    auto token = consume(); // consume import
    Token path = consume();
    if(path.type != token::STRING) {
        auto error = CompileError(ErrorType::FileError, "Not a filepath.","path",path);
        error.add_message("Import statements must be followed by a string literal representing the file path to import in POSIX format.");
        return Result<std::unique_ptr<PreNodeImport>>(error);
    }
    std::string filepath = StringUtils::remove_quotes(path.val);
    std::unique_ptr<PreNodeKeyword> alias;
    if(peek().type == token::AS) {
        auto error = CompileError(ErrorType::ParseError, "", "", peek());
        error.set_message("Importing file with alias is not supported in as of version " + COMPILER_VERSION + ". "+
                          "Please remove the 'as <alias>' part from the import statement.");
        error.exit();

        consume(); // consume <as> token
        auto alias_keyword = parse_keyword(parent);
        if (alias_keyword.is_error()) {
            auto error = alias_keyword.get_error();
            error.add_message("Import statements with alias must be in the format: import \"<filepath>\" as <alias>, where <alias> is a single keyword token.");
            return Result<std::unique_ptr<PreNodeImport>>(error);
        }
        alias = std::move(alias_keyword.unwrap());
    }
    auto end_token = peek(-1);
    if(peek().type != token::LINEBRK) {
        auto error = CompileError(ErrorType::ParseError, "Incorrect <import> syntax.","linebreak",peek());
        error.add_message("Import statements must end with a linebreak after the file path.");
        return Result<std::unique_ptr<PreNodeImport>>(error);

    }
    consume(); //consume linebreak
    auto import_statement = std::make_unique<PreNodeImport>(filepath, token, parent);
    import_statement->set_alias(std::move(alias));
    import_statement->set_range(token, end_token);
    return Result<std::unique_ptr<PreNodeImport>>(std::move(import_statement));
}

Result<std::unique_ptr<PreNodeImportNCKP>> PreprocessorParser::parse_import_nckp(PreNodeAST *parent) {
    auto token = consume(); // consume import_nckp
    if(peek().type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<PreNodeImportNCKP>>(CompileError(ErrorType::ParseError,
        "Incorrect import_nckp Syntax.","(",peek()));
    }
    consume(); // consume (
    if(peek().type != token::STRING) {
        return Result<std::unique_ptr<PreNodeImportNCKP>>(CompileError(ErrorType::PreprocessorError,
        "Not a filepath","path",peek()));
    }
    Token path = consume(); // consume filepath
    std::string filepath = StringUtils::remove_quotes(path.val);
    if(peek().type != token::CLOSED_PARENTH) {
        return Result<std::unique_ptr<PreNodeImportNCKP>>(CompileError(ErrorType::ParseError,
        "Incorrect import_nckp Syntax.",")",peek()));
    }
    auto end_token = consume(); // consume )
    if(peek().type != token::LINEBRK)
        return Result<std::unique_ptr<PreNodeImportNCKP>>(CompileError(ErrorType::ParseError,
        "Incorrect import Syntax.","linebreak",peek()));
    consume(); //consume linebreak
    auto return_value = std::make_unique<PreNodeImportNCKP>(filepath, token, parent);
    return_value->set_range(token, end_token);
    return Result<std::unique_ptr<PreNodeImportNCKP>>(std::move(return_value));
}

Result<std::unique_ptr<PreNodeSetCondition>> PreprocessorParser::parse_set_condition(PreNodeAST *parent) {
    auto error_msg = "Found invalid <condition> syntax. ";
    auto token = consume(); // consume SET_CONDITION
    if(peek().type != token::OPEN_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"(",peek());
        error.add_message("<condition> statements must be in the format: SET_CONDITION(<condition-symbol>)");
        return Result<std::unique_ptr<PreNodeSetCondition>>(error);
    }
    consume(); // consume (
    if(peek().type != token::KEYWORD) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"<condition-symbol>",peek());
        error.add_message("<condition> statements must be in the format: SET_CONDITION(<condition-symbol>), where <condition-symbol> is a single keyword token representing the symbol to set for conditional compilation.");
        return Result<std::unique_ptr<PreNodeSetCondition>>(error);
    }
    auto condition = parse_keyword(parent);
    if (condition.is_error()) {
        auto error = condition.get_error();
        error.add_message("<condition> statements must be in the format: SET_CONDITION(<condition-symbol>), where <condition-symbol> is a single keyword token representing the symbol to set for conditional compilation.");
        return Result<std::unique_ptr<PreNodeSetCondition>>(condition.get_error());
    }
    if(peek().type != token::CLOSED_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,")",peek());
        error.add_message("<condition> statements must be in the format: SET_CONDITION(<condition-symbol>).");
        return Result<std::unique_ptr<PreNodeSetCondition>>(error);
    }
    auto end_token = consume(); // consume )
    if(peek().type != token::LINEBRK) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"linebreak",peek());
        error.add_message("<condition> statements must end with a linebreak after the closing parenthesis.");
        return Result<std::unique_ptr<PreNodeSetCondition>>(error);
    }
    consume(); // consume linebreak
    auto node_set_condition = std::make_unique<PreNodeSetCondition>(std::move(condition.unwrap()), token, parent);
    node_set_condition->set_range(token, end_token);
    return Result<std::unique_ptr<PreNodeSetCondition>>(std::move(node_set_condition));
}

Result<std::unique_ptr<PreNodeResetCondition>> PreprocessorParser::parse_reset_condition(PreNodeAST *parent) {
    auto token = consume(); // consume RESET_CONDITION
    auto error_msg = "Found invalid <condition> syntax. ";
    if(peek().type != token::OPEN_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"(",peek());
        error.add_message("<condition> statements must be in the format: RESET_CONDITION(<condition-symbol>)");
        return Result<std::unique_ptr<PreNodeResetCondition>>(error);
    }
    consume(); // consume (
    if(peek().type != token::KEYWORD) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"<condition-symbol>",peek());
        error.add_message("<condition> statements must be in the format: RESET_CONDITION(<condition-symbol>), where <condition-symbol> is a single keyword token representing the symbol to reset for conditional compilation.");
        return Result<std::unique_ptr<PreNodeResetCondition>>(error);
    }
    auto condition = parse_keyword(parent);
    if (condition.is_error()) {
        auto error = condition.get_error();
        error.add_message("<condition> statements must be in the format: RESET_CONDITION(<condition-symbol>), where <condition-symbol> is a single keyword token representing the symbol to reset for conditional compilation.");
        return Result<std::unique_ptr<PreNodeResetCondition>>(condition.get_error());
    }
    if(peek().type != token::CLOSED_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,")",peek());
        error.add_message("<condition> statements must be in the format: RESET_CONDITION(<condition-symbol>).");
        return Result<std::unique_ptr<PreNodeResetCondition>>(error);
    }
    auto end_token = consume(); // consume )
    if(peek().type != token::LINEBRK) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"linebreak",peek());
        error.add_message("<condition> statements must end with a linebreak after the closing parenthesis.");
        return Result<std::unique_ptr<PreNodeResetCondition>>(error);
    }
    consume(); // consume linebreak
    auto node_reset_condition = std::make_unique<PreNodeResetCondition>(std::move(condition.unwrap()), token, parent);
    node_reset_condition->set_range(token, end_token);
    return Result<std::unique_ptr<PreNodeResetCondition>>(std::move(node_reset_condition));
}

Result<std::unique_ptr<PreNodeUseCodeIf>> PreprocessorParser::parse_use_code_if(PreNodeAST *parent) {
    auto token = consume(); // consume USE_CODE_IF or USE_CODE_IF_NOT
    bool use_if = token.type == token::USE_CODE_IF; // will be false if token::USE_CODE_IF_NOT
    auto error_msg = "Found invalid <USE_CODE_IF> syntax. ";
    auto node_if = std::make_unique<PreNodeUseCodeIf>(token, parent);
    if(peek().type != token::OPEN_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"(",peek());
        error.add_message("<USE_CODE_IF> statements must be in the format: USE_CODE_IF(<condition-symbol>)");
        return Result<std::unique_ptr<PreNodeUseCodeIf>>(error);
    }
    consume(); // consume (
    auto condition_result = parse_keyword(node_if.get());
    if (condition_result.is_error()) {
        auto error = condition_result.get_error();
        error.add_message("<USE_CODE_IF> statements must be in the format: USE_CODE_IF(<condition-symbol>), where <condition-symbol> is a single keyword token representing the symbol to check for conditional compilation.");
        return Result<std::unique_ptr<PreNodeUseCodeIf>>(condition_result.get_error());
    }
    auto condition = std::move(condition_result.unwrap());
    if(peek().type != token::CLOSED_PARENTH) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,")",peek());
        error.add_message("<USE_CODE_IF> statements must be in the format: USE_CODE_IF(<condition-symbol>).");
        return Result<std::unique_ptr<PreNodeUseCodeIf>>(error);
    }
    auto end_token = consume(); // consume )
    if(peek().type != token::LINEBRK) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg,"linebreak",peek());
        error.add_message("<USE_CODE_IF> statements must end with a linebreak after the closing parenthesis.");
        return Result<std::unique_ptr<PreNodeUseCodeIf>>(error);
    }
    consume(); // consume linebreak
    auto node_chunk = std::make_unique<PreNodeChunk>(peek(), node_if.get());
    while (peek().type != token::END_USE_CODE) {
        if(peek().type == token::END_USE_CODE) break;
        if(peek().type == token::END_TOKEN) {
            auto error = CompileError(ErrorType::PreprocessorError, error_msg, "end", peek());
            error.set_message("Missing <END_USE_CODE> statement. Reached the end of the file while parsing an preprocessor if statement.");
            return Result<std::unique_ptr<PreNodeUseCodeIf>>(error);
        }
        if (is_define_definition()) {
            auto result_define = parse_define_definition(parent);
            if (result_define.is_error())
                return Result<std::unique_ptr<PreNodeUseCodeIf>>(result_define.get_error());
            m_program->define_statements.push_back(std::move(result_define.unwrap()));
        } else {
            auto token_result = parse_token(parent);
            if (token_result.is_error())
                return Result<std::unique_ptr<PreNodeUseCodeIf>>(token_result.get_error());
            node_chunk->chunk.push_back(std::move(token_result.unwrap()));
        }
    }
    auto end_inc = consume(); // consume END_INC

    if (peek().type != token::LINEBRK) {
        auto error = CompileError(ErrorType::PreprocessorError, error_msg, "linebreak", peek());
        error.set_message("Missing linebreak after <END_USE_CODE> statement.");
        return Result<std::unique_ptr<PreNodeUseCodeIf>>(error);
    }
    consume(); // consume linebreak
    node_if->set_condition(std::move(condition));
    if (use_if) {
        node_if->set_if_branch(std::move(node_chunk));
    } else {
        node_if->set_else_branch(std::move(node_chunk));
    }
    return Result<std::unique_ptr<PreNodeUseCodeIf>>(std::move(node_if));
}

Result<std::unique_ptr<PreNodePragma>> PreprocessorParser::parse_pragma(PreNodeAST* parent) {
    auto token = consume(); // consume #pragma
    auto node_pragma = std::make_unique<PreNodePragma>(token, parent);
    std::string pragma_error_msg = "Unable to process #pragma syntax.";
    if(peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<PreNodePragma>>(CompileError(ErrorType::PreprocessorError,
    pragma_error_msg, token.line, "Valid pragma option.",token.val, token.file));
    }
    auto node_option = parse_keyword(node_pragma.get());
    if(peek().type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<PreNodePragma>>(CompileError(ErrorType::PreprocessorError, pragma_error_msg, token.line, "(",token.val, token.file));
    }
    consume(); // consume (
    if(peek().type != token::KEYWORD and peek().type != token::STRING and peek().type != token::INT and peek().type != token::TRUE and peek().type != token::FALSE) {
        return Result<std::unique_ptr<PreNodePragma>>(CompileError(ErrorType::PreprocessorError, pragma_error_msg, token.line, "Valid pragma parameter in <string>, <integer>, <bool> format.",token.val, token.file));
    }
    auto result_keyword = parse_keyword(node_pragma.get());
    if (result_keyword.is_error())
        return Result<std::unique_ptr<PreNodePragma>>(result_keyword.get_error());
    auto node_parameter = std::move(result_keyword.unwrap());

    if(peek().type != token::CLOSED_PARENTH) {
        CompileError(ErrorType::PreprocessorError, pragma_error_msg, token.line, ")",token.val, token.file).exit();
    }
    auto end_token = consume(); // consume )
    if(peek().type != token::LINEBRK) {
        CompileError(ErrorType::PreprocessorError, pragma_error_msg, token.line, "linebreak",token.val, token.file).exit();
    }
    consume(); // consume \n
    node_pragma->option = std::move(node_option.unwrap());
    node_pragma->argument = std::move(node_parameter);
    node_pragma->set_range(token, end_token);
    return Result<std::unique_ptr<PreNodePragma>>(std::move(node_pragma));
}






