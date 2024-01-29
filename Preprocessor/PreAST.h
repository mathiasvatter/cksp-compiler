//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include <utility>

#include "../Tokenizer/Tokenizer.h"
#include "../AST/AST.h"

struct PreNodeAST {
	PreNodeAST* parent = nullptr;
	explicit PreNodeAST(PreNodeAST* parent= nullptr) : parent(parent) {}
    virtual ~PreNodeAST() = default;
    virtual void accept(class PreASTVisitor& visitor) {}
    // Virtuelle clone()-Methode für tiefe Kopien
    [[nodiscard]] virtual std::unique_ptr<PreNodeAST> clone() const = 0;
    virtual void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) {};
    void replace_with(std::unique_ptr<PreNodeAST> newNode);
    virtual void update_parents(PreNodeAST* new_parent) {
        parent = new_parent;
    }
    virtual void update_token_data(const Token &token) = 0;
	virtual std::string get_string() = 0;
};

struct PreNodeNumber : PreNodeAST {
    Token number;
    PreNodeNumber(Token tok, PreNodeAST* parent) : PreNodeAST(parent), number(std::move(tok)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeNumber(const PreNodeNumber& other) : PreNodeAST(other), number(other.number) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	std::string get_string() override {
		return number.val;
	}
    void update_token_data(const Token &token) override {
        number.line = token.line; number.file = token.file;
    }
};

struct PreNodeInt : PreNodeAST {
    int32_t integer;
    Token number;
    PreNodeInt(int32_t integer, Token number, PreNodeAST* parent) : PreNodeAST(parent), number(std::move(number)), integer(integer) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeInt(const PreNodeInt& other) : PreNodeAST(other), integer(other.integer), number(other.number) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	std::string get_string() override {
		return number.val;
	}
    void update_token_data(const Token &token) override {
        number.line = token.line; number.file = token.file;
    }
};

struct PreNodeUnaryExpr : PreNodeAST {
    Token op;
    std::unique_ptr<PreNodeAST> operand;
    PreNodeUnaryExpr(Token op, std::unique_ptr<PreNodeAST> operand, PreNodeAST *parent) : PreNodeAST(parent), op(std::move(op)), operand(std::move(operand)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeUnaryExpr(const PreNodeUnaryExpr& other) : PreNodeAST(other), op(other.op), operand(clone_unique(other.operand)) {}
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        operand->update_parents(this);
    }
	std::string get_string() override {
		return op.val + operand->get_string();
	}
    void update_token_data(const Token &token) override {
        op.line = token.line; op.file = token.file;
        operand->update_token_data(token);
    }
};

struct PreNodeBinaryExpr: PreNodeAST {
    std::unique_ptr<PreNodeAST> left, right;
    Token op;
    PreNodeBinaryExpr(Token op, std::unique_ptr<PreNodeAST> left, std::unique_ptr<PreNodeAST> right, PreNodeAST *parent)
            : PreNodeAST(parent), op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeBinaryExpr(const PreNodeBinaryExpr& other) : PreNodeAST(other), left(clone_unique(other.left)), right(
            clone_unique(other.right)), op(other.op) {}
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        left->update_parents(this);
        right->update_parents(this);
    }
	std::string get_string() override {
		return left->get_string() + op.val + left->get_string();;
	}
    void update_token_data(const Token &token) override {
        op.line = token.line; op.file = token.file;
        left->update_token_data(token);
        right->update_token_data(token);
    }
};

struct PreNodeKeyword : PreNodeAST {
    Token keyword;
	PreNodeKeyword(Token tok, PreNodeAST *parent) : PreNodeAST(parent), keyword(std::move(tok)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeKeyword(const PreNodeKeyword& other) : PreNodeAST(other), keyword(other.keyword) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	std::string get_string() override {
		return keyword.val;
	}
    void update_token_data(const Token &token) override {
        keyword.line = token.line; keyword.file = token.file;
    }
};

struct PreNodeOther : PreNodeAST {
    Token other;
	PreNodeOther(Token tok, PreNodeAST *parent) : PreNodeAST(parent), other(std::move(tok)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeOther(const PreNodeOther& other) : PreNodeAST(other), other(other.other) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	std::string get_string() override {
		return other.val;
	}
    void update_token_data(const Token &token) override {
        other.line = token.line; other.file = token.file;
    }
};

struct PreNodeDeadCode : PreNodeAST {
    Token sth;
    PreNodeDeadCode(Token tok, PreNodeAST *parent) : PreNodeAST(parent), sth(std::move(tok)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeDeadCode(const PreNodeDeadCode& other) : PreNodeAST(other), sth(other.sth) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    std::string get_string() override {
        return sth.val;
    }
    void update_token_data(const Token &token) override {
        sth.line = token.line; sth.file = token.file;
    }
};

struct PreNodePragma : PreNodeAST {
    std::unique_ptr<PreNodeKeyword> option;
    std::unique_ptr<PreNodeKeyword> argument;
    PreNodePragma(std::unique_ptr<PreNodeKeyword> opt, std::unique_ptr<PreNodeKeyword> arg, PreNodeAST *parent)
    : PreNodeAST(parent), option(std::move(opt)), argument(std::move(arg)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodePragma(const PreNodePragma& other)
    : PreNodeAST(other), option(clone_unique(other.option)), argument(clone_unique(other.argument)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    std::string get_string() override {
        return option->get_string();
    }
    void update_token_data(const Token &token) override {
        option->update_token_data(token);
        argument->update_token_data(token);
    }
};

struct PreNodeStatement : PreNodeAST {
    std::unique_ptr<PreNodeAST> statement;
    PreNodeStatement(std::unique_ptr<PreNodeAST> statement, PreNodeAST *parent) : PreNodeAST(parent), statement(std::move(statement)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeStatement(const PreNodeStatement& other) : PreNodeAST(other), statement(clone_unique(other.statement)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
    void update_parents(PreNodeAST* new_parent) override {
        statement->update_parents(this);
    }
	std::string get_string() override {
		return statement->get_string();
	}
    void update_token_data(const Token &token) override {
        statement->update_token_data(token);
    }
};

struct PreNodeChunk : PreNodeAST {
	std::vector<std::unique_ptr<PreNodeAST>> chunk = {};
    PreNodeChunk(std::vector<std::unique_ptr<PreNodeAST>> chunk, PreNodeAST *parent) : PreNodeAST(parent), chunk(std::move(chunk)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeChunk(const PreNodeChunk& other) : PreNodeAST(other), chunk(clone_vector(other.chunk)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        for(auto &c : chunk) {
            c->update_parents(this);
        }
    }
	std::string get_string() override {
		std::string str;
		for(auto &c : chunk) {
			str += c->get_string();
		}
		return str;
	}
    void update_token_data(const Token &token) override {
        for(auto & c : chunk) {
            c->update_token_data(token);
        }
    }
};

struct PreNodeList : PreNodeAST {
    std::vector<std::unique_ptr<PreNodeChunk>> params;
    PreNodeList(std::vector<std::unique_ptr<PreNodeChunk>> params, PreNodeAST *parent) : PreNodeAST(parent), params(std::move(params)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeList(const PreNodeList& other) : PreNodeAST(other), params(clone_vector(other.params)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        for(auto &p : params) {
            p->update_parents(this);
        }
    }
	std::string get_string() override {
		std::string str;
		for(auto & p : params) {
			str += p->get_string();
		}
		return str;
	}
    void update_token_data(const Token &token) override {
        for(auto & p : params) {
            p->update_token_data(token);
        }
    }
};

struct PreNodeMacroHeader : PreNodeAST {
    std::unique_ptr<PreNodeKeyword> name;
    std::unique_ptr<PreNodeList> args;
    PreNodeMacroHeader(std::unique_ptr<PreNodeKeyword> name, std::unique_ptr<PreNodeList> args, PreNodeAST *parent)
    : PreNodeAST(parent), name(std::move(name)), args(std::move(args)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeMacroHeader(const PreNodeMacroHeader& other) : PreNodeAST(other), name(clone_unique(other.name)), args(clone_unique(other.args)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        args->update_parents(this);
        name->update_parents(this);
    }
	std::string get_string() override {
		return name->get_string() + args->get_string();
	}
    void update_token_data(const Token &token) override {
        name->update_token_data(token);
        args->update_token_data(token);
    }
};

struct PreNodeDefineHeader : PreNodeAST {
    std::unique_ptr<PreNodeKeyword> name;
    std::unique_ptr<PreNodeList> args;
	PreNodeDefineHeader(std::unique_ptr<PreNodeKeyword> name, std::unique_ptr<PreNodeList> args, PreNodeAST *parent)
		: PreNodeAST(parent), name(std::move(name)), args(std::move(args)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeDefineHeader(const PreNodeDefineHeader& other) : PreNodeAST(other), name(clone_unique(other.name)), args(clone_unique(other.args)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        args->update_parents(this);
    }
	std::string get_string() override {
		return name->get_string() + args->get_string();
	}
    void update_token_data(const Token &token) override {
        name->update_token_data(token);
        args->update_token_data(token);
    }
};

struct PreNodeMacroDefinition : PreNodeAST {
    std::unique_ptr<PreNodeMacroHeader> header;
    std::unique_ptr<PreNodeChunk> body;
    PreNodeMacroDefinition(std::unique_ptr<PreNodeMacroHeader> header, std::unique_ptr<PreNodeChunk> body, PreNodeAST* parent)
            : PreNodeAST(parent), header(std::move(header)), body(std::move(body)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeMacroDefinition(const PreNodeMacroDefinition& other) : PreNodeAST(other), header(clone_unique(other.header)), body(clone_unique(other.body)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        header->update_parents(this);
        body->update_parents(this);
    }
	std::string get_string() override {
		return header->get_string() + body->get_string();
	}
    void update_token_data(const Token &token) override {
        header->update_token_data(token);
        body->update_token_data(token);
    }
};

struct PreNodeDefineStatement : PreNodeAST {
    std::unique_ptr<PreNodeDefineHeader> header;
    std::unique_ptr<PreNodeChunk> body;
    PreNodeDefineStatement(std::unique_ptr<PreNodeDefineHeader> header, std::unique_ptr<PreNodeChunk> body, PreNodeAST* parent)
    : PreNodeAST(parent), header(std::move(header)), body(std::move(body)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeDefineStatement(const PreNodeDefineStatement& other) : PreNodeAST(other), header(clone_unique(other.header)), body(clone_unique(other.body)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        header->update_parents(this);
        body->update_parents(this);
    }
	std::string get_string() override {
		return header->get_string() + body->get_string();
	}
    void update_token_data(const Token &token) override {
        header->update_token_data(token);
        body->update_token_data(token);
    }
};

struct PreNodeMacroCall : PreNodeAST {
    std::unique_ptr<PreNodeMacroHeader> macro;
    PreNodeMacroCall(std::unique_ptr<PreNodeMacroHeader> macro, PreNodeAST* parent) : PreNodeAST(parent), macro(std::move(macro)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeMacroCall(const PreNodeMacroCall& other) : PreNodeAST(other), macro(clone_unique(other.macro)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        macro->update_parents(this);
    }
	std::string get_string() override {
		return macro->get_string();
	}
    void update_token_data(const Token &token) override {
        macro->update_token_data(token);
    }
};

struct PreNodeDefineCall : PreNodeAST {
    std::unique_ptr<PreNodeDefineHeader> define;
    PreNodeDefineCall(std::unique_ptr<PreNodeDefineHeader> define, PreNodeAST* parent) : PreNodeAST(parent), define(std::move(define)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeDefineCall(const PreNodeDefineCall& other) : PreNodeAST(other), define(clone_unique(other.define)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        define->update_parents(this);
    }
	std::string get_string() override {
		return define->get_string();
	}
    void update_token_data(const Token &token) override {
        define->update_token_data(token);
    }
};

struct PreNodeIterateMacro : PreNodeAST {
    std::unique_ptr<PreNodeList> macro_call;
    std::unique_ptr<PreNodeChunk> iterator_start;
    Token to;
    std::unique_ptr<PreNodeChunk> iterator_end;
    std::unique_ptr<PreNodeChunk> step;
    explicit PreNodeIterateMacro(PreNodeAST *parent) : PreNodeAST(parent) {};
    PreNodeIterateMacro(std::unique_ptr<PreNodeList> macroCall, std::unique_ptr<PreNodeChunk> iteratorStart,
                            Token to, std::unique_ptr<PreNodeChunk> iteratorEnd, std::unique_ptr<PreNodeChunk> step, PreNodeAST *parent)
                            : PreNodeAST(parent), macro_call(std::move(macroCall)),
                            iterator_start(std::move(iteratorStart)), to(std::move(to)), iterator_end(std::move(iteratorEnd)), step(std::move(step)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeIterateMacro(const PreNodeIterateMacro& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        macro_call->update_parents(this);
        iterator_start->update_parents(this);
        iterator_end->update_parents(this);
        step->update_parents(this);
    }
	std::string get_string() override {
		return macro_call->get_string() + iterator_start->get_string() + to.val + iterator_end->get_string() + step->get_string();
	}
    void update_token_data(const Token &token) override {
        macro_call->update_token_data(token);
        iterator_start->update_token_data(token);
        to.line = token.line; to.file = token.file;
        iterator_end->update_token_data(token);
        step->update_token_data(token);
    }
};

struct PreNodeLiterateMacro : PreNodeAST {
    std::unique_ptr<PreNodeList> macro_call;
    std::unique_ptr<PreNodeChunk> literate_tokens;
    PreNodeLiterateMacro(std::unique_ptr<PreNodeList> macro_call, std::unique_ptr<PreNodeChunk> literate_tokens, PreNodeAST *parent) :
            PreNodeAST(parent), macro_call(std::move(macro_call)), literate_tokens(std::move(literate_tokens)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeLiterateMacro(const PreNodeLiterateMacro& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        macro_call->update_parents(this);
        literate_tokens->update_parents(this);
    }
	std::string get_string() override {
		return macro_call->get_string() + literate_tokens->get_string();
	}
    void update_token_data(const Token &token) override {
        macro_call->update_token_data(token);
        literate_tokens->update_token_data(token);
    }
};

struct PreNodeIncrementer : PreNodeAST {
    Token tok;
    std::vector<std::unique_ptr<PreNodeChunk>> body;
    std::unique_ptr<PreNodeAST> counter;
    std::unique_ptr<PreNodeChunk> iterator_start;
    std::unique_ptr<PreNodeChunk> iterator_step;
    std::vector<bool> incrementation;
    PreNodeIncrementer(std::vector<std::unique_ptr<PreNodeChunk>> body, std::unique_ptr<PreNodeAST> counter, std::unique_ptr<PreNodeChunk> iterator_start, std::unique_ptr<PreNodeChunk> iterator_step, Token tok, std::vector<bool> incrementation, PreNodeAST *parent) :
    PreNodeAST(parent), incrementation(std::move(incrementation)), tok(std::move(tok)), body(std::move(body)), counter(std::move(counter)), iterator_start(std::move(iterator_start)), iterator_step(std::move(iterator_step)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeIncrementer(const PreNodeIncrementer& other);
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        for (auto & b : body) {
            b->update_parents(this);
        }
        counter->update_parents(this);
        iterator_start->update_parents(this);
        iterator_step->update_parents(this);
    }
    std::string get_string() override {
        return counter->get_string() + iterator_start->get_string() + iterator_step->get_string();
    }
    void update_token_data(const Token &token) override {
        tok.line = token.line; tok.file = token.file;
        for(auto & b : body) {
            b->update_token_data(token);
        }
        counter->update_token_data(token);
        iterator_start->update_token_data(token);
        iterator_step->update_token_data(token);
    }
};

struct PreNodeProgram : PreNodeAST {
    std::vector<std::unique_ptr<PreNodeAST>> program;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> define_statements;
    std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions;
    PreNodeProgram(std::vector<std::unique_ptr<PreNodeAST>> program, std::vector<std::unique_ptr<PreNodeDefineStatement>> defines,
                   std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions, PreNodeAST* parent)
    : PreNodeAST(parent), program(std::move(program)), define_statements(std::move(defines)), macro_definitions(std::move(macro_definitions)) {}
    void accept(PreASTVisitor& visitor) override;
    void replace_child(PreNodeAST* oldChild, std::unique_ptr<PreNodeAST> newChild) override;
    PreNodeProgram(const PreNodeProgram& other) : PreNodeAST(other),
    program(clone_vector(other.program)), define_statements(clone_vector(other.define_statements)) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
    void update_parents(PreNodeAST* new_parent) override {
        parent = new_parent;
        for(auto & p : program) p->update_parents(this);
        for(auto & def : define_statements) def ->update_parents(this);
        for(auto & def : macro_definitions) def ->update_parents(this);
    }
	std::string get_string() override {
		return "";
	}
    void update_token_data(const Token &token) override {}
};



