//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include <utility>

#include "../Tokenizer/Tokenizer.h"

struct PreNodeAST {
	PreNodeAST* parent = nullptr;
	explicit PreNodeAST(PreNodeAST* parent= nullptr) : parent(parent) {}
};

struct PreNodeNumber : PreNodeAST {
    Token number;
	explicit PreNodeNumber(Token tok, PreNodeAST* parent= nullptr) : PreNodeAST(parent), number(std::move(tok)) {}
};

struct PreNodeKeyword : PreNodeAST {
    Token keyword;
	explicit PreNodeKeyword(Token tok, PreNodeAST *parent) : PreNodeAST(parent), keyword(std::move(tok)) {}
};

struct PreNodeOther : PreNodeAST {
    Token other;
	PreNodeOther(Token tok, PreNodeAST *parent) : PreNodeAST(parent), other(std::move(tok)) {}
};

struct PreNodeChunk : PreNodeAST {
	std::vector<PreNodeAST> chunk;
    PreNodeChunk(std::vector<PreNodeAST> chunk, PreNodeAST *parent) : PreNodeAST(parent), chunk(std::move(chunk)) {}
};

struct PreNodeDefineHeader : PreNodeAST {
	std::string name;
	std::vector<std::vector<Token>> args;
	PreNodeDefineHeader(std::string name, std::vector<std::vector<Token>> args, PreNodeAST *parent)
		: PreNodeAST(parent), name(std::move(name)), args(std::move(args)) {}
};

struct PreNodeDefineStatement : PreNodeAST {
    PreNodeDefineHeader header;
    PreNodeChunk body;
    explicit PreNodeDefineStatement(PreNodeDefineHeader header, PreNodeChunk body)
    : header(std::move(header)), body(std::move(body)) {}
};

struct PreNodeDefineCall : PreNodeAST {
    PreNodeDefineHeader define;
    explicit PreNodeDefineCall(PreNodeDefineHeader define) : define(std::move(define)) {}
};


