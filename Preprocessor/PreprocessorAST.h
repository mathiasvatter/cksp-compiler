//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include <utility>

#include "Tokenizer/Tokenizer.h"

struct PreNodeAST {
	Token token;
	PreNodeAST* parent = nullptr;
	explicit PreNodeAST(Token tok, PreNodeAST* parent= nullptr) : token(std::move(tok)), parent(parent) {}
};

struct PreNodeNumber : PreNodeAST {
	explicit PreNodeNumber(const Token &tok, PreNodeAST* parent= nullptr) : PreNodeAST(tok, parent) {}
};

struct PreNodeKeyword : PreNodeAST {
	explicit PreNodeKeyword(const Token &tok, PreNodeAST *parent) : PreNodeAST(tok, parent) {}
};

struct PreNodeOther : PreNodeAST {
	PreNodeOther(const Token &tok, PreNodeAST *parent) : PreNodeAST(tok, parent) {}
};

struct PreNodeChunk : PreNodeAST {
	std::vector<PreNodeAST>
};

struct PreNodeDefineHeader : PreNodeAST {
	std::string name;
	std::vector<std::vector<Token>> args;
	PreNodeDefineHeader(std::string name, std::vector<std::vector<Token>> args, Token tok)
		: NodeAST(tok), name(std::move(name)), args(std::move(args)) {}
	NodeDefineHeader(const NodeDefineHeader& other);
	std::unique_ptr<NodeAST> clone() const override;
};


