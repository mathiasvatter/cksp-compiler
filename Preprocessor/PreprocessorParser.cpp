//
// Created by Mathias Vatter on 10.11.23.
//

#include "PreprocessorParser.h"

PreprocessorParser::PreprocessorParser(std::vector<Token> tokens, const std::string &currentFile) : Preprocessor(
        std::move(tokens), currentFile) {
    m_pos = 0;
    m_curr_token = m_tokens.at(0).type;
}

Result<std::unique_ptr<PreNodeProgram>> PreprocessorParser::parse_program(PreNodeAST *parent) {
    std::vector<std::unique_ptr<PreNodeAST>> program;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> define_statements;
    auto node_program = std::make_unique<PreNodeProgram>(std::move(program), std::move(define_statements), nullptr);
    // get definitions first
    while(peek(m_tokens).type != END_TOKEN) {
        if (is_define_definition()) {
            consume(m_tokens); // consume define
            m_define_statements.push_back(consume(m_tokens).val);
        } else consume(m_tokens);
    }
    // get construct calls
    m_pos = 0;
    while(peek(m_tokens).type != END_TOKEN) {
        if (is_define_definition()) {
            auto result_define = parse_define_definition(parent);
            if (result_define.is_error())
                return Result<std::unique_ptr<PreNodeProgram>>(result_define.get_error());
            node_program->define_statements.push_back(std::move(result_define.unwrap()));
        } else {
            auto token_result = parse_token(parent);
            if (token_result.is_error())
                return Result<std::unique_ptr<PreNodeProgram>>(token_result.get_error());
            node_program->program.push_back(std::move(token_result.unwrap()));
        }
    }
    auto node_end_token = std::make_unique<PreNodeOther>(std::move(consume(m_tokens)), parent);
    node_program->program.push_back(std::move(node_end_token));
    return Result<std::unique_ptr<PreNodeProgram>>(std::move(node_program));
}


bool PreprocessorParser::is_define_definition() {
    if(m_pos >0)
        return m_pos > 0 and peek(m_tokens).type == DEFINE and peek(m_tokens, -1).type == LINEBRK;
    else
        return peek(m_tokens).type == DEFINE;
}

bool PreprocessorParser::is_define_call(const Token &tok) {
    bool syntax = false;
    if(m_pos >0)
        syntax = (peek(m_tokens).type == KEYWORD and m_pos > 0 and (peek(m_tokens, -1).type != MACRO or peek(m_tokens, -1).type != FUNCTION));
    else
        syntax = (peek(m_tokens).type == KEYWORD);

    if(syntax) {
        //search in m_define_statements
//        auto it = std::find_if(m_define_statements.begin(), m_define_statements.end(),
//                               [&](const std::unique_ptr<PreNodeDefineStatement> &statement) {
//                                   return statement->header->name == tok.val;
//                               });
        auto it = std::find(m_define_statements.begin(), m_define_statements.end(),
                               tok.val);
        syntax &= it != m_define_statements.end();
    }
    return syntax;
}


Result<std::unique_ptr<PreNodeAST>> PreprocessorParser::parse_token(PreNodeAST* parent) {
    std::unique_ptr<PreNodeAST> stmt;
    auto node_statement = std::make_unique<PreNodeStatement>(nullptr, parent);
    if(is_define_call(peek(m_tokens))) {
        auto result_define_call = parse_define_call(node_statement.get());
        if(result_define_call.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_define_call.get_error());
        node_statement->statement = std::move(result_define_call.unwrap());
        stmt = std::move(node_statement);
    } else if(peek(m_tokens).type == KEYWORD) {
        auto result_keyword = parse_keyword(parent);
        if(result_keyword.is_error())
            return Result<std::unique_ptr<PreNodeAST>>(result_keyword.get_error());
        node_statement->statement = std::move(result_keyword.unwrap());
        stmt = std::move(node_statement);
    } else if(peek(m_tokens).type == INT or peek(m_tokens).type == FLOAT or peek(m_tokens).type == BINARY or peek(m_tokens).type == HEXADECIMAL) {
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
    auto node_number = std::make_unique<PreNodeNumber>(consume(m_tokens), parent);
    return Result<std::unique_ptr<PreNodeNumber>>(std::move(node_number));
}

Result<std::unique_ptr<PreNodeKeyword>> PreprocessorParser::parse_keyword(PreNodeAST *parent) {
    auto node_keyword = std::make_unique<PreNodeKeyword>(consume(m_tokens), parent);
    return Result<std::unique_ptr<PreNodeKeyword>>(std::move(node_keyword));
}

Result<std::unique_ptr<PreNodeOther>> PreprocessorParser::parse_other(PreNodeAST *parent) {
    auto node_other = std::make_unique<PreNodeOther>(consume(m_tokens), parent);
    return Result<std::unique_ptr<PreNodeOther>>(std::move(node_other));
}

Result<std::unique_ptr<PreNodeList>> PreprocessorParser::parse_list(PreNodeAST *parent) {
    std::vector<std::unique_ptr<PreNodeChunk>> params_list = {};
    auto node_list = std::make_unique<PreNodeList>(std::move(params_list), parent);
    if (peek(m_tokens).type != token::OPEN_PARENTH) {
        return Result<std::unique_ptr<PreNodeList>>(CompileError(ErrorType::PreprocessorError,
        "Missing open parenthesis.",peek(m_tokens).line,"(",peek(m_tokens).val, peek(m_tokens).file));
    }
    consume(m_tokens); // consume (
    if (peek(m_tokens).type != token::CLOSED_PARENTH) {
        int parenth_depth = 1; // Start with 1 because we've already consumed the first OPEN_PARENTH
        auto node_chunk = std::make_unique<PreNodeChunk>(std::move(std::vector<std::unique_ptr<PreNodeAST>>{}), node_list.get());
        while (parenth_depth > 0) {
            if (peek(m_tokens).type == token::OPEN_PARENTH) {
                parenth_depth++;
            } else if (peek(m_tokens).type == token::CLOSED_PARENTH) {
                parenth_depth--;
            } else if (peek(m_tokens).type == token::END_TOKEN) {
                return Result<std::unique_ptr<PreNodeList>>(CompileError(ErrorType::PreprocessorError,
            "Unexpected end of file. Missing closing parenthesis.",peek(m_tokens).line, ")", peek(m_tokens).val,peek(m_tokens).file));
            }
            if (peek(m_tokens).type == token::COMMA && parenth_depth == 1) {
                node_list->params.push_back(std::move(node_chunk));
                node_chunk = std::make_unique<PreNodeChunk>(std::move(std::vector<std::unique_ptr<PreNodeAST>>{}), node_list.get());
                consume(m_tokens); // consume COMMA
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
    consume(m_tokens); //consume )
    return Result<std::unique_ptr<PreNodeList>>(std::move(node_list));
}

//Result<std::unique_ptr<PreNodeDefineCall>> PreprocessorParser::parse_define_call(PreNodeAST *parent) {
//    return Result<std::unique_ptr<PreNodeDefineCall>>(__1::unique_ptr());
//}

Result<std::unique_ptr<PreNodeDefineHeader>> PreprocessorParser::parse_define_header(PreNodeAST *parent) {
    Token name = consume(m_tokens);
//    std::unique_ptr<PreNodeList> args;
    auto node_define_header = std::make_unique<PreNodeDefineHeader>(std::move(name), nullptr, parent);
    std::unique_ptr<PreNodeList> define_args = std::make_unique<PreNodeList>(std::vector<std::unique_ptr<PreNodeChunk>>{}, node_define_header.get());
    if(peek(m_tokens).type == OPEN_PARENTH) {
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

    consume(m_tokens); //consume define
    if (peek(m_tokens).type != token::KEYWORD) {
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Missing define name.",peek(m_tokens).line,"<keyword>",peek(m_tokens).val, peek(m_tokens).file));
    }
    auto define_header_result = parse_define_header(define_statement.get());
    if(define_header_result.is_error())
        return Result<std::unique_ptr<PreNodeDefineStatement>>(define_header_result.get_error());

    if(peek(m_tokens).type != ASSIGN)
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Found invalid Define Statement Syntax. Missing <assign> symbol.", peek(m_tokens).line, ":=", peek(m_tokens).val, peek(m_tokens).file));
    consume(m_tokens); //consume :=

    std::vector<std::unique_ptr<PreNodeAST>> assignee = {};
    auto node_chunk = std::make_unique<PreNodeChunk>(std::move(assignee), parent);
    while(peek(m_tokens).type != LINEBRK) {
        if (peek(m_tokens).type == token::END_TOKEN)
            return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
         "Unexpected end of m_tokens. Missing assignment of define statement.",peek(m_tokens).line, "", peek(m_tokens).val,peek(m_tokens).file));

        if(peek(m_tokens).type == KEYWORD and define_header_result.unwrap()->name.val == peek(m_tokens).val) {
            return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::SyntaxError,
	    "A define constant cannot define itself.",peek(m_tokens).line,"","", peek(m_tokens).file));
		}

        auto result_token = parse_token(node_chunk.get());
        if(result_token.is_error())
            return Result<std::unique_ptr<PreNodeDefineStatement>>(result_token.get_error());
        node_chunk->chunk.push_back(std::move(result_token.unwrap()));
    }
    if(node_chunk->chunk.empty()) {
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Found empty define statement assignment.",peek(m_tokens).line, "", peek(m_tokens).val,peek(m_tokens).file));
    }
    if (peek(m_tokens).type != token::LINEBRK) {
        return Result<std::unique_ptr<PreNodeDefineStatement>>(CompileError(ErrorType::PreprocessorError,
     "Missing necessary linebreak after define statement.",peek(m_tokens).line,"linebreak",peek(m_tokens).val, peek(m_tokens).file));
    }
    consume(m_tokens); //consume linebreak
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





