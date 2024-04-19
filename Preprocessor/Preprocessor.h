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
    const std::vector<std::unique_ptr<DataStructure>> &get_external_variables() const;
    const std::string &get_output_path() const;

protected:
    std::string m_current_file;
    std::string m_output_path;

    // gets filled by import preprocessor
    std::vector<std::unique_ptr<DataStructure>> m_external_variables;

    void remove_tokens(std::vector<Token>& tok, size_t start, size_t end);
	static size_t search(const std::vector<std::string>& vec, const std::string& str);

	[[nodiscard]] Token peek(const std::vector<Token>& tok, int ahead = 0);
	Token consume(const std::vector<Token>& tok);
	[[nodiscard]] const Token& get_tok(const std::vector<Token>& tok) const;
    std::string token_vector_to_string(const std::vector<Token>& tokens);

};





