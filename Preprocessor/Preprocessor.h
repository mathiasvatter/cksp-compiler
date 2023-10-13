//
// Created by Mathias Vatter on 23.09.23.
//

#pragma once
#include <unordered_set>
#include <utility>

#include "../Tokenizer/Tokenizer.h"
#include "../AST.h"
#include "../Parser.h"

class Preprocessor : public Parser {
public:
    Preprocessor(std::vector<Token> tokens, std::string current_file);
	~Preprocessor() = default;
    std::vector<Token> get_tokens();
    void process();
protected:
    std::string m_current_file;

    void remove_last();
    void remove_tokens(std::vector<Token>& tok, size_t start, size_t end);

	[[nodiscard]] Token peek(const std::vector<Token>& tok, int ahead = 0);
	Token consume(const std::vector<Token>& tok);
	const Token& get_tok(const std::vector<Token>& tok) const;

	Result<std::unique_ptr<NodeAST>> parse_int(const std::vector<Token>& tok);
	Result<std::unique_ptr<NodeAST>> parse_binary_expr(const std::vector<Token>& tok);
	Result<std::unique_ptr<NodeAST>> _parse_primary_expr(const std::vector<Token>& tok);
	Result<std::unique_ptr<NodeAST>> parse_unary_expr(const std::vector<Token>& tok);
	Result<std::unique_ptr<NodeAST>> _parse_binary_expr_rhs(int precedence, std::unique_ptr<NodeAST> lhs, const std::vector<Token>& tok);
	/// ( expression )
	Result<std::unique_ptr<NodeAST>> _parse_parenth_expr(const std::vector<Token>& tok);
};


class SimpleInterpreter {
public:
	explicit SimpleInterpreter() {}
	Result<int> evaluate_int_expression(std::unique_ptr<NodeAST>& root);
};



