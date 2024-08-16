//
// Created by Mathias Vatter on 08.11.23.
//

#pragma once

#include <utility>

#include "../../Tokenizer/Tokenizer.h"
#include "../../AST/ASTNodes/AST.h"

enum class PreNodeType {
	NUMBER,
	INT,
	UNARY_EXPR,
	BINARY_EXPR,
	KEYWORD,
	OTHER,
	DEAD_CODE,
	PRAGMA,
	STATEMENT,
	CHUNK,
	DEFINE_HEADER,
	LIST,
	DEFINE_STATEMENT,
	DEFINE_CALL,
	MACRO_DEFINITION,
	MACRO_HEADER,
	MACRO_CALL,
	FUNCTION_CALL,
	ITERATE_MACRO,
	LITERATE_MACRO,
	INCREMENTER,
	PROGRAM
};

struct PreNodeAST {
//	Token tok;
	PreNodeType type;
	PreNodeAST* parent = nullptr;
	explicit PreNodeAST(PreNodeAST* parent=nullptr, PreNodeType type=PreNodeType::DEAD_CODE) : parent(parent), type(type) {}
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
	PreNodeType get_node_type() const { return type; }
};

// Template-Funktion für sicheren Cast
template<typename T>
T* safe_cast(PreNodeAST* node, PreNodeType type) {
	if (node && node->get_node_type() == type) {  // Überprüfen des Typs
		return static_cast<T*>(node);
	}
	return nullptr;
}

template<typename T>
std::unique_ptr<T> clone_as(PreNodeAST* node) {
	return std::unique_ptr<T>(static_cast<T*>(node->clone().release()));
}

struct PreNodeLiteral : PreNodeAST {
	Token value;
	PreNodeLiteral(Token tok, PreNodeAST* parent, PreNodeType type=PreNodeType::DEAD_CODE) : PreNodeAST(parent, type), value(std::move(tok)) {}
	PreNodeLiteral(const PreNodeLiteral& other) : PreNodeAST(other), value(other.value) {}
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	std::string get_string() override {
		return value.val;
	}
	void update_token_data(const Token &token) override {
		value.line = token.line; value.file = token.file;
	}
};

struct PreNodeNumber : PreNodeLiteral {
    PreNodeNumber(Token tok, PreNodeAST* parent) : PreNodeLiteral(tok, parent, PreNodeType::NUMBER) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeNumber(const PreNodeNumber& other) : PreNodeLiteral(other) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
};

struct PreNodeInt : PreNodeLiteral {
    int32_t integer;
    PreNodeInt(int32_t integer, Token number, PreNodeAST* parent) : PreNodeLiteral(number, parent, PreNodeType::INT), integer(integer) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeInt(const PreNodeInt& other) : PreNodeLiteral(other), integer(other.integer) {}
    [[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
};

struct PreNodeKeyword : PreNodeLiteral {
	PreNodeKeyword(Token tok, PreNodeAST *parent) : PreNodeLiteral(tok, parent, PreNodeType::KEYWORD) {}
	void accept(PreASTVisitor& visitor) override;
	PreNodeKeyword(const PreNodeKeyword& other) : PreNodeLiteral(other) {}
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
};

struct PreNodeUnaryExpr : PreNodeAST {
    Token op;
    std::unique_ptr<PreNodeAST> operand;
    PreNodeUnaryExpr(Token op, std::unique_ptr<PreNodeAST> operand, PreNodeAST *parent)
		: PreNodeAST(parent, PreNodeType::UNARY_EXPR), op(std::move(op)), operand(std::move(operand)) {}
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
            : PreNodeAST(parent, PreNodeType::BINARY_EXPR), op(std::move(op)), left(std::move(left)), right(std::move(right)) {}
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

struct PreNodeOther : PreNodeAST {
    Token other;
	PreNodeOther(Token tok, PreNodeAST *parent) : PreNodeAST(parent, PreNodeType::OTHER), other(std::move(tok)) {}
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
    PreNodeDeadCode(Token tok, PreNodeAST *parent) : PreNodeAST(parent, PreNodeType::DEAD_CODE), sth(std::move(tok)) {}
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
    	: PreNodeAST(parent, PreNodeType::PRAGMA), option(std::move(opt)), argument(std::move(arg)) {}
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
    PreNodeStatement(std::unique_ptr<PreNodeAST> statement, PreNodeAST *parent)
		: PreNodeAST(parent, PreNodeType::STATEMENT), statement(std::move(statement)) {}
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
    PreNodeChunk(std::vector<std::unique_ptr<PreNodeAST>> chunk, PreNodeAST *parent)
		: PreNodeAST(parent, PreNodeType::CHUNK), chunk(std::move(chunk)) {}
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
	/// puts nested statement list in one, returns new vector to replace node->statements with
	inline void flatten() {
		std::vector<std::unique_ptr<PreNodeAST>> temp;
		temp.reserve(chunk.size()); // Speicherreservierung um unnötige Allokationen zu vermeiden

		for (auto& i : chunk) {
			if (auto node_statement = safe_cast<PreNodeStatement>(i.get(), PreNodeType::STATEMENT)) {
				if (auto node_chunk = safe_cast<PreNodeChunk>(node_statement->statement.get(), PreNodeType::CHUNK)) {
					// Fügen Sie die inneren Statements zum temporären Vector hinzu
					auto& inner_chunk = node_chunk->chunk;
					temp.insert(temp.end(),
								std::make_move_iterator(inner_chunk.begin()),
								std::make_move_iterator(inner_chunk.end()));
					continue; // Weiter zur nächsten Iteration
				}
			}
			// Fügen Sie das aktuelle Element zum temporären Vector hinzu
			temp.push_back(std::move(i));
		}
		chunk = std::move(temp);
	}
};

struct PreNodeList : PreNodeAST {
    std::vector<std::unique_ptr<PreNodeChunk>> params;
    PreNodeList(std::vector<std::unique_ptr<PreNodeChunk>> params, PreNodeAST *parent)
		: PreNodeAST(parent, PreNodeType::LIST), params(std::move(params)) {}
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
	bool has_parenth = false;
    PreNodeMacroHeader(std::unique_ptr<PreNodeKeyword> name, std::unique_ptr<PreNodeList> args, PreNodeAST *parent)
    : PreNodeAST(parent, PreNodeType::MACRO_HEADER), name(std::move(name)), args(std::move(args)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeMacroHeader(const PreNodeMacroHeader& other) : PreNodeAST(other), has_parenth(other.has_parenth), name(clone_unique(other.name)), args(clone_unique(other.args)) {}
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
		: PreNodeAST(parent, PreNodeType::DEFINE_HEADER), name(std::move(name)), args(std::move(args)) {}
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
		: PreNodeAST(parent, PreNodeType::MACRO_DEFINITION), header(std::move(header)), body(std::move(body)) {}
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
    : PreNodeAST(parent, PreNodeType::DEFINE_STATEMENT), header(std::move(header)), body(std::move(body)) {}
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
	bool is_iterate_macro = false;
    PreNodeMacroCall(std::unique_ptr<PreNodeMacroHeader> macro, PreNodeAST* parent)
		: PreNodeAST(parent, PreNodeType::MACRO_CALL), macro(std::move(macro)) {}
    void accept(PreASTVisitor& visitor) override;
    PreNodeMacroCall(const PreNodeMacroCall& other) : PreNodeAST(other), is_iterate_macro(other.is_iterate_macro), macro(clone_unique(other.macro)) {}
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

struct PreNodeFunctionCall : PreNodeAST {
	std::unique_ptr<PreNodeMacroHeader> function;
	PreNodeFunctionCall(std::unique_ptr<PreNodeMacroHeader> function, PreNodeAST* parent)
		: PreNodeAST(parent, PreNodeType::FUNCTION_CALL), function(std::move(function)) {}
	void accept(PreASTVisitor& visitor) override;
	PreNodeFunctionCall(const PreNodeFunctionCall& other) : PreNodeAST(other), function(clone_unique(other.function)) {}
	[[nodiscard]] std::unique_ptr<PreNodeAST> clone() const override;
	void update_parents(PreNodeAST* new_parent) override {
		parent = new_parent;
		function->update_parents(this);
	}
	std::string get_string() override {
		return function->get_string();
	}
	void update_token_data(const Token &token) override {
		function->update_token_data(token);
	}
};

struct PreNodeDefineCall : PreNodeAST {
    std::unique_ptr<PreNodeDefineHeader> define;
    PreNodeDefineCall(std::unique_ptr<PreNodeDefineHeader> define, PreNodeAST* parent)
		: PreNodeAST(parent, PreNodeType::DEFINE_CALL), define(std::move(define)) {}
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
				: PreNodeAST(parent, PreNodeType::ITERATE_MACRO), macro_call(std::move(macroCall)),
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
            PreNodeAST(parent, PreNodeType::LITERATE_MACRO), macro_call(std::move(macro_call)), literate_tokens(std::move(literate_tokens)) {}
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
    PreNodeIncrementer(std::vector<std::unique_ptr<PreNodeChunk>> body, std::unique_ptr<PreNodeAST> counter, std::unique_ptr<PreNodeChunk> iterator_start, std::unique_ptr<PreNodeChunk> iterator_step, Token tok, std::vector<bool> incrementation, PreNodeAST *parent)
		: PreNodeAST(parent, PreNodeType::INCREMENTER), incrementation(std::move(incrementation)), tok(std::move(tok)), body(std::move(body)), counter(std::move(counter)), iterator_start(std::move(iterator_start)), iterator_step(std::move(iterator_step)) {}
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
	std::stack<PreNodeDefineStatement*> define_call_stack;
	std::stack<PreNodeMacroCall*> macro_call_stack;
    std::vector<std::unique_ptr<PreNodeAST>> program;
    std::vector<std::unique_ptr<PreNodeDefineStatement>> define_statements;
    std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions;
    PreNodeProgram(std::vector<std::unique_ptr<PreNodeAST>> program, std::vector<std::unique_ptr<PreNodeDefineStatement>> defines,
                   std::vector<std::unique_ptr<PreNodeMacroDefinition>> macro_definitions, PreNodeAST* parent)
    	: PreNodeAST(parent, PreNodeType::PROGRAM), program(std::move(program)), define_statements(std::move(defines)), macro_definitions(std::move(macro_definitions)) {}
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



