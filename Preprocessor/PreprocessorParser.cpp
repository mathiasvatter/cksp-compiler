//
// Created by Mathias Vatter on 10.11.23.
//

#include "PreprocessorParser.h"

PreprocessorParser::PreprocessorParser(std::vector<Token> tokens) : Parser(
        std::move(tokens), std::move(std::vector<std::string>())) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
}

Result<std::unique_ptr<PreNodeProgram>> PreprocessorParser::parse_program(PreNodeAST *parent) {
    std::vector<std::unique_ptr<PreNodeAST>> program;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> define_statements;
    std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions;
    auto node_program = std::make_unique<PreNodeProgram>(std::move(program), std::move(define_statements), std::move(macro_definitions), nullptr);
    // get definitions first
    while(peek().type != END_TOKEN) {
        if (is_define_definition()) {
            consume(); // consume define
            m_define_statements.push_back(consume().val);
        } else if (is_macro_definition()) {
            consume(); // consume macro
            m_macro_definitions.push_back(consume().val);
        } else consume();
    }
    // get construct calls
    m_pos = 0;
    while(peek().type != END_TOKEN) {
        if (is_define_definition()) {
            auto result_define = parse_define_definition(parent);
            if (result_define.is_error())
                return Result<std::unique_ptr<PreNodeProgram>>(result_define.get_error());
            node_program->define_statements.push_back(std::move(result_define.unwrap()));
        } else if (is_macro_definition()) {
            auto result_macro_def = parse_macro_definition(parent);
            if (result_macro_def.is_error())
                return Result<std::unique_ptr<PreNodeProgram>>(result_macro_def.get_error());
            node_program->macro_definitions.push_back(std::move(result_macro_def.unwrap()));
        } else {
            auto token_result = parse_token(parent);
            if (token_result.is_error())
                return Result<std::unique_ptr<PreNodeProgram>>(token_result.get_error());
            node_program->program.push_back(std::move(token_result.unwrap()));
        }
    }
    auto node_end_token = std::make_unique<PreNodeOther>(std::move(consume()), parent);
    node_program->program.push_back(std::move(node_end_token));
    return Result<std::unique_ptr<PreNodeProgram>>(std::move(node_program));
}


bool PreprocessorParser::is_define_definition() {
    if(m_pos >0)
        return m_pos > 0 and peek().type == DEFINE and peek(-1).type == LINEBRK;
    else
        return peek().type == DEFINE;
}

bool PreprocessorParser::is_macro_definition() {
    if(m_pos >0)
        return m_pos > 0 and peek().type == MACRO and peek(-1).type == LINEBRK;
    else
        return peek().type == MACRO;
}

bool PreprocessorParser::is_func_call(const Token &tok, const std::vector<std::string> &definitions) {
    bool syntax = false;
    if(m_pos >0)
        syntax = (peek().type == KEYWORD and m_pos > 0 and (peek(-1).type != MACRO or peek(-1).type != FUNCTION));
    else
        syntax = (peek().type == KEYWORD);

    if(syntax) {
        //search in m_define_statements
//        auto it = std::find_if(m_define_statements.begin(), m_define_statements.end(),
//                               [&](const std::unique_ptr<PreNodeDefineStatement> &statement) {
//                                   return statement->header->name == tok.val;
//                               });
        auto it = std::find(definitions.begin(), definitions.end(),
                               tok.val);
        syntax &= it != definitions.end();
    }
    return syntax;
}

bool PreprocessorParser::is_macro_call(const Token &tok) {
    bool syntax = false;
    if(m_pos >0)
        syntax = peek().type == KEYWORD and m_pos > 0 and peek(-1).type == LINEBRK and (peek(1).type == OPEN_PARENTH or peek(1).type == LINEBRK);
    else
        syntax = peek().type == KEYWORD and peek(1).type == LINEBRK;

    if(syntax) {
        //search in m_define_statements
//        auto it = std::find_if(m_define_statements.begin(), m_define_statements.end(),
//                               [&](const std::unique_ptr<PreNodeDefineStatement> &statement) {
//                                   return statement->header->name == tok.val;
//                               });
        auto it = std::find(m_macro_definitions.begin(), m_macro_definitions.end(),
                            tok.val);
        syntax &= it != m_macro_definitions.end();
    }
    return syntax;
}


Result<std::unique_ptr<PreNodeAST>> PreprocessorParser::parse_token(PreNodeAST* parent) {
    std::unique_ptr<PreNodeAST> stmt;
    auto node_statement = std::make_unique<PreNodeStatement>(nullptr, parent);
    if(is_func_call(peek(), m_define_statements)) {
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
    } else if(peek().type == ITERATE_MACRO) {
        auto result_iterate_macro = parse_iterate_macro(node_statement.get());
        if(result_iterate_macro.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_iterate_macro.get_error());
        node_statement->statement = std::move(result_iterate_macro.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == LITERATE_MACRO) {
        auto result_literate_macro = parse_literate_macro(node_statement.get());
        if(result_literate_macro.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_literate_macro.get_error());
        node_statement->statement = std::move(result_literate_macro.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == KEYWORD or peek().type == STRING) {
        auto result_keyword = parse_keyword(parent);
        if(result_keyword.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_keyword.get_error());
        node_statement->statement = std::move(result_keyword.unwrap());
        stmt = std::move(node_statement);
    } else if(peek().type == INT or peek().type == FLOAT or peek().type == BINARY or peek().type == HEXADECIMAL) {
        auto result_number = parse_number(parent);
        if (result_number.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_number.get_error());
        node_statement->statement = std::move(result_number.unwrap());
        stmt = std::move(node_statement);
    } else {
        auto result_other = parse_other(parent);
        if(result_other.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_other.get_error());
        stmt = std::move(result_other.unwrap());
    }
    return Result<std::unique_ptr<PreNodeAST>>(std::move(stmt));
}

Result<std::unique_ptr<PreNodeNumber>> PreprocessorParser::parse_number(PreNodeAST *parent) {
    auto node_number = std::make_unique<PreNodeNumber>(consume(), parent);
    return Result<std::unique_ptr<PreNodeNumber>>(std::move(node_number));
}

Result<std::unique_ptr<PreNodeKeyword>> PreprocessorParser::parse_keyword(PreNodeAST *parent) {
    auto node_keyword = std::make_unique<PreNodeKeyword>(consume(), parent);
    return Result<std::unique_ptr<PreNodeKeyword>>(std::move(node_keyword));
}

Result<std::unique_ptr<PreNodeOther>> PreprocessorParser::parse_other(PreNodeAST *parent) {
    auto node_other = std::make_unique<PreNodeOther>(consume(), parent);
    return Result<std::unique_ptr<PreNodeOther>>(std::move(node_other));
}

Result<std::unique_ptr<PreNodeList>> PreprocessorParser::parse_list(PreNodeAST *parent) {
    std::vector<std::unique_ptr<PreNodeChunk>> params_list = {};
    auto node_list = std::make_unique<PreNodeList>(std::move(params_list), parent);
    if (peek().type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<PreNodeList>>(CompileError(ErrorType::PreprocessorError,
        "Missing open parenthesis.",peek().line,"(",peek().val, peek().file));
    }
    consume(); // consume (
    if (peek().type != token::CLOSED_PARENTH) {
        int parenth_depth = 1; // Start with 1 because we've already consumed the first OPEN_PARENTH
        auto node_chunk = std::make_unique<PreNodeChunk>(std::move(std::vector<std::unique_ptr<PreNodeAST>>{}), node_list.get());
        while (parenth_depth > 0) {
            if (peek().type == token::OPEN_PARENTH) {
                parenth_depth++;
            } else if (peek().type == token::CLOSED_PARENTH) {
                parenth_depth--;
            } else if (peek().type == token::END_TOKEN) {
                return Result<std::unique_ptr<PreNodeList>>(CompileError(ErrorType::PreprocessorError,
            "Unexpected end of file. Missing closing parenthesis.",peek().line, ")", peek().val,peek().file));
            }
            if (peek().type == token::COMMA && parenth_depth == 1) {
                node_list->params.push_back(std::move(node_chunk));
                node_chunk = std::make_unique<PreNodeChunk>(std::move(std::vector<std::unique_ptr<PreNodeAST>>{}), node_list.get());
                consume(); // consume COMMA
            } else if(parenth_depth > 0) {
                auto result_token = parse_token(node_chunk.get());
                if(result_token.is_error())
                    return Result<std::unique_ptr<PreNodeList>>(result_token.get_error());
                node_chunk->chunk.push_back(std::move(result_token.unwrap()));
            }
        }
        if (node_chunk != nullptr) {
            node_list->params.push_back(std::move(node_chunk));
        }
    }
    consume(); //consume )
    return Result<std::unique_ptr<PreNodeList>>(std::move(node_list));
}

Result<std::unique_ptr<PreNodeDefineHeader>> PreprocessorParser::parse_define_header(PreNodeAST *parent) {
    Token name = consume();
//    std::unique_ptr<PreNodeList> args;
    auto node_define_header = std::make_unique<PreNodeDefineHeader>(std::move(name), nullptr, parent);
    std::unique_ptr<PreNodeList> define_args = std::make_unique<PreNodeList>(std::vector<std::unique_ptr<PreNodeChunk>>{}, node_define_header.get());
    if(peek().type == OPEN_PARENTH) {
        auto define_args_result = parse_list(node_define_header.get());
        if (define_args_result.is_error())
            return Result<std::unique_ptr<PreNodeDefineHeader>>(define_args_result.get_error());
        define_args = std::move(define_args_result.unwrap());
    }
    node_define_header->args = std::move(define_args);
    return Result<std::unique_ptr<PreNodeDefineHeader>>(std::move(node_define_header));
}

Result<std::unique_ptr<PreNodeDefineStatement>> PreprocessorParser::parse_define_definition(PreNodeAST *parent) {
    auto define_statement = std::make_unique<PreNodeDefineStatement>(nullptr, nullptr, parent);

    consume(); //consume define
    if (peek().type != token::KEYWORD) {
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Missing define name.",peek().line,"<keyword>",peek().val, peek().file));
    }
    auto define_header_result = parse_define_header(define_statement.get());
    if(define_header_result.is_error())
        return Result<std::unique_ptr<PreNodeDefineStatement>>(define_header_result.get_error());

    if(peek().type != ASSIGN)
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Found invalid Define Statement Syntax. Missing <assign> symbol.", peek().line, ":=", peek().val, peek().file));
    consume(); //consume :=

    std::vector<std::unique_ptr<PreNodeAST>> assignee = {};
    auto node_chunk = std::make_unique<PreNodeChunk>(std::move(assignee), parent);
    while(peek().type != LINEBRK) {
        if (peek().type == token::END_TOKEN)
            return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
         "Unexpected end of m_tokens. Missing assignment of define statement.",peek().line, "", peek().val,peek().file));

        if(peek().type == KEYWORD and define_header_result.unwrap()->name.val == peek().val) {
            return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	    "A define constant cannot define itself.",peek().line,"","", peek().file));
		}

        auto result_token = parse_token(node_chunk.get());
        if(result_token.is_error())
            return Result<std::unique_ptr<PreNodeDefineStatement>>(result_token.get_error());
        node_chunk->chunk.push_back(std::move(result_token.unwrap()));
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
    return Result<std::unique_ptr<PreNodeDefineStatement>>(std::move(define_statement));
}

Result<std::unique_ptr<PreNodeDefineCall>> PreprocessorParser::parse_define_call(PreNodeAST *parent) {
    auto node_define_call = std::make_unique<PreNodeDefineCall>(nullptr, parent);
    auto define_header_result = parse_define_header(node_define_call.get());
    if(define_header_result.is_error())
        return Result<std::unique_ptr<PreNodeDefineCall>>(define_header_result.get_error());
    node_define_call->define = std::move(define_header_result.unwrap());

    return Result<std::unique_ptr<PreNodeDefineCall>>(std::move(node_define_call));
}

Result<std::unique_ptr<PreNodeMacroHeader>> PreprocessorParser::parse_macro_header(PreNodeAST* parent) {
    auto macro_name = consume();
    auto node_macro_header = std::make_unique<PreNodeMacroHeader>(std::move(macro_name), nullptr, parent);
    std::unique_ptr<PreNodeList> macro_args = std::make_unique<PreNodeList>(std::move(std::vector<std::unique_ptr<PreNodeChunk>>{}), node_macro_header.get());

    if (peek().type == token::OPEN_PARENTH) {
        auto macro_args_result = parse_list(node_macro_header.get());
        if (macro_args_result.is_error())
            return Result<std::unique_ptr<PreNodeMacroHeader>>(macro_args_result.get_error());
        macro_args = std::move(macro_args_result.unwrap());
    }
    node_macro_header->args = std::move(macro_args);
    node_macro_header->args->parent = node_macro_header.get();
    return Result<std::unique_ptr<PreNodeMacroHeader>>(std::move(node_macro_header));
}

Result<std::unique_ptr<PreNodeMacroCall>> PreprocessorParser::parse_macro_call(PreNodeAST* parent) {
    auto node_macro_call = std::make_unique<PreNodeMacroCall>(nullptr, parent);
    auto macro_stmt = parse_macro_header(node_macro_call.get());
    if(macro_stmt.is_error()){
        return Result<std::unique_ptr<PreNodeMacroCall>>(macro_stmt.get_error());
    }
    node_macro_call->macro = std::move(macro_stmt.unwrap());
    return Result<std::unique_ptr<PreNodeMacroCall>>(std::move(node_macro_call));
}

Result<std::unique_ptr<PreNodeMacroDefinition>> PreprocessorParser::parse_macro_definition(PreNodeAST* parent) {
    auto node_macro_definition = std::make_unique<PreNodeMacroDefinition>(nullptr, nullptr, parent);
    consume(); // consume macro
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
        return Result<std::unique_ptr<PreNodeMacroDefinition>>(CompileError(ErrorType::SyntaxError,
     "Missing necessary linebreak after macro header.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak
    auto node_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node_macro_definition.get());
    while (peek().type != token::END_MACRO) {
        if(peek().type == END_MACRO) break;

        if (peek().type == token::MACRO) {
            return Result<std::unique_ptr<PreNodeMacroDefinition>>(CompileError(ErrorType::PreprocessorError,
         "Nested macros are not allowed. Maybe you forgot an 'end macro' along the line?",peek().line,"",peek().val, peek().file));
        } else {
            auto result_token = parse_token(node_chunk.get());
            if(result_token.is_error())
                return Result<std::unique_ptr<PreNodeMacroDefinition>>(result_token.get_error());
            node_chunk->chunk.push_back(std::move(result_token.unwrap()));
        }
    }
    consume(); // consume end macro
    node_macro_definition->body = std::move(node_chunk);
    return Result<std::unique_ptr<PreNodeMacroDefinition>>(std::move(node_macro_definition));
}

Result<std::unique_ptr<PreNodeIterateMacro>> PreprocessorParser::parse_iterate_macro(PreNodeAST* parent) {
    auto node_iterate_macro = std::make_unique<PreNodeIterateMacro>(parent);
    consume(); // consume iterate_macro
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
    SimpleInterpreter evaluator;
    auto iterator_start =  parse_binary_expr(nullptr); //_parse_iterator_start();
    if(iterator_start.is_error())
        return Result<std::unique_ptr<PreNodeIterateMacro>>(iterator_start.get_error());
    auto it_start = evaluator.evaluate_int_expression(iterator_start.unwrap());
    if(it_start.is_error())
        return Result<std::unique_ptr<PreNodeIterateMacro>>(it_start.get_error());

    if(not(peek().type == token::TO or peek().type == token::DOWNTO)) {
        return Result<std::unique_ptr<PreNodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
      "Found invalid <iterate_macro> statement syntax.", peek().line,"<to>/<downto>", peek().val, peek().file));
    }
    Token to = consume(); // consume downto/to

    auto iterator_end =  parse_binary_expr(nullptr); //_parse_iterator_end();
    if(iterator_end.is_error())
        return Result<std::unique_ptr<PreNodeIterateMacro>>(iterator_end.get_error());
    auto it_end = evaluator.evaluate_int_expression(iterator_end.unwrap());
    if(it_end.is_error())
        return Result<std::unique_ptr<PreNodeIterateMacro>>(it_end.get_error());

    int step = 1;
    if(peek().type == STEP) {
        consume(); // consume step
        auto step_result = parse_binary_expr(nullptr);
        if(step_result.is_error())
            return Result<std::unique_ptr<PreNodeIterateMacro>>(step_result.get_error());
        auto step_expression = evaluator.evaluate_int_expression(step_result.unwrap());
        if(step_expression.is_error())
            return Result<std::unique_ptr<PreNodeIterateMacro>>(step_expression.get_error());
        step = step_expression.unwrap();
    }
    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<PreNodeIterateMacro>>(CompileError(ErrorType::SyntaxError,
    "Missing necessary linebreak after <iterate_macro> statement.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak
    node_iterate_macro->macro_call = std::move(node_statement.unwrap());
    node_iterate_macro->iterator_start = it_start.unwrap();
    node_iterate_macro->iterator_end = it_end.unwrap();
    node_iterate_macro -> to = to;
    node_iterate_macro->step = step;
    return Result<std::unique_ptr<PreNodeIterateMacro>>(std::move(node_iterate_macro));
}

Result<std::unique_ptr<PreNodeLiterateMacro>> PreprocessorParser::parse_literate_macro(PreNodeAST* parent) {
    auto node_literate_macro = std::make_unique<PreNodeLiterateMacro>(nullptr, nullptr, parent);
    consume(); // consume literate_macro
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
    auto node_chunk = std::make_unique<PreNodeChunk>(std::vector<std::unique_ptr<PreNodeAST>>{}, node_literate_macro.get());
    while(peek().type != LINEBRK) {
        if(peek().type == KEYWORD or peek().type == STRING) {
            auto result_token = parse_token(node_chunk.get());
            if (result_token.is_error())
                return Result<std::unique_ptr<PreNodeLiterateMacro>>(result_token.get_error());
            node_chunk->chunk.push_back(std::move(result_token.unwrap()));
        } else {
            return Result<std::unique_ptr<PreNodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
          "Found invalid <literate_macro> statement syntax. Can only literate on <keywords>.",peek().line,"<keyword>",peek().val, peek().file));
        }
        if(peek().type == COMMA)
            consume();
    }

    if (peek().type != token::LINEBRK) {
        return Result<std::unique_ptr<PreNodeLiterateMacro>>(CompileError(ErrorType::SyntaxError,
     "Missing necessary linebreak after <literate_macro> statement.",peek().line,"linebreak",peek().val, peek().file));
    }
    consume(); // consume linebreak

    node_literate_macro->macro_call = std::move(node_statement.unwrap());
    node_literate_macro->literate_tokens = std::move(node_chunk);
    return Result<std::unique_ptr<PreNodeLiterateMacro>>(std::move(node_literate_macro));
}





