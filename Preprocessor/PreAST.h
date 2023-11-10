//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include <utility>

#include "../Tokenizer/Tokenizer.h"

struct PreNodeAST {
	PreNodeAST* parent = nullptr;
	explicit PreNodeAST(PreNodeAST* parent= nullptr) : parent(parent) {}
    virtual void accept(class PreASTVisitor& visitor) {}
    virtual void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) {};
    void replace_with(std::unique_ptr<PreNodeAST> newNode);
};

struct PreNodeNumber : PreNodeAST {
    Token number;
    PreNodeNumber(Token tok, PreNodeAST* parent) : PreNodeAST(parent), number(std::move(tok)) {}
    void accept(PreASTVisitor& visitor) override;
};

struct PreNodeKeyword : PreNodeAST {
    Token keyword;
	PreNodeKeyword(Token tok, PreNodeAST *parent) : PreNodeAST(parent), keyword(std::move(tok)) {}
    void accept(PreASTVisitor& visitor) override;
};

struct PreNodeOther : PreNodeAST {
    Token other;
	PreNodeOther(Token tok, PreNodeAST *parent) : PreNodeAST(parent), other(std::move(tok)) {}
    void accept(PreASTVisitor& visitor) override;
};

struct PreNodeStatement : PreNodeAST {
    std::unique_ptr<PreNodeAST> statement;
    PreNodeStatement(std::unique_ptr<PreNodeAST> statement, PreNodeAST *parent) : PreNodeAST(parent), statement(std::move(statement)) {}
    void accept(PreASTVisitor& visitor) override;
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
};

struct PreNodeChunk : PreNodeAST {
	std::vector<std::unique_ptr<PreNodeAST>> chunk;
    PreNodeChunk(std::vector<std::unique_ptr<PreNodeAST>> chunk, PreNodeAST *parent) : PreNodeAST(parent), chunk(std::move(chunk)) {}
    void accept(PreASTVisitor& visitor) override;
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
};

struct PreNodeList : PreNodeAST {
    std::vector<std::unique_ptr<PreNodeChunk>> params;
    PreNodeList(std::vector<std::unique_ptr<PreNodeChunk>> params, PreNodeAST *parent) : PreNodeAST(parent), params(std::move(params)) {}
    void accept(PreASTVisitor& visitor) override;
};

struct PreNodeDefineHeader : PreNodeAST {
	std::string name;
    std::unique_ptr<PreNodeList> args;
	PreNodeDefineHeader(std::string name, std::unique_ptr<PreNodeList> args, PreNodeAST *parent)
		: PreNodeAST(parent), name(std::move(name)), args(std::move(args)) {}
    void accept(PreASTVisitor& visitor) override;
};

struct PreNodeDefineStatement : PreNodeAST {
    std::unique_ptr<PreNodeDefineHeader> header;
    std::unique_ptr<PreNodeChunk> body;
    PreNodeDefineStatement(std::unique_ptr<PreNodeDefineHeader> header, std::unique_ptr<PreNodeChunk> body, PreNodeAST* parent)
    : PreNodeAST(parent), header(std::move(header)), body(std::move(body)) {}
    void accept(PreASTVisitor& visitor) override;
};

struct PreNodeDefineCall : PreNodeAST {
    std::unique_ptr<PreNodeDefineHeader> define;
    PreNodeDefineCall(std::unique_ptr<PreNodeDefineHeader> define, PreNodeAST* parent) : PreNodeAST(parent), define(std::move(define)) {}
    void accept(PreASTVisitor& visitor) override;
};

struct PreNodeProgram : PreNodeAST {
    std::vector<std::unique_ptr<PreNodeAST>> program;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> define_statements;
    PreNodeProgram(std::vector<std::unique_ptr<PreNodeAST>> program, std::vector<std::unique_ptr<PreNodeDefineStatement>> defines, PreNodeAST* parent)
    : PreNodeAST(parent), program(std::move(program)), define_statements(std::move(defines)) {}
    void accept(PreASTVisitor& visitor) override;
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
};



