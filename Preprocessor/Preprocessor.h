//
// Created by Mathias Vatter on 23.09.23.
//

#pragma once
#include <unordered_set>
#include <utility>

#include "../Tokenizer/Tokenizer.h"
#include "../AST/AST.h"
#include "../Parser.h"

class Preprocessor : public Parser {
public:
    Preprocessor(std::vector<Token> tokens, std::string current_file);
	~Preprocessor() = default;
    std::vector<Token> get_tokens();
    void process();

    const std::vector<std::string> &get_macro_definitions() const;

protected:
    std::string m_current_file;

	// define collection
	std::vector<std::unique_ptr<NodeDefineStatement>> m_define_statements;
	std::vector<std::string> m_define_strings;
	// macro collection
	std::vector<std::unique_ptr<NodeMacroDefinition>> m_macro_definitions;
    std::vector<std::string> m_macro_evaluation_stack;


    void remove_last();
    void remove_tokens(std::vector<Token>& tok, size_t start, size_t end);
	static size_t search(const std::vector<std::string>& vec, const std::string& str);

	[[nodiscard]] Token peek(const std::vector<Token>& tok, int ahead = 0);
	Token consume(const std::vector<Token>& tok);
	[[nodiscard]] const Token& get_tok(const std::vector<Token>& tok) const;
    std::string token_vector_to_string(const std::vector<Token>& tokens);
	static Result<SuccessTag> substitute_macro_params(std::vector<Token>& macro_body, const std::vector<std::vector<Token>>& placeholders, const std::vector<std::vector<Token>>& new_args);
	Result<std::vector<std::vector<Token>>> parse_nested_params_list(std::vector<Token>& tok);
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



