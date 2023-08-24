//
// Created by Mathias Vatter on 03.06.23.
//

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <map>

#include "Tokens.h"

/*
 * Callbacks
 * Variables
 * Operator
 * Control Statement
 *
 */

struct Token {
    Token(token type, const std::string &val, size_t line);
    friend std::ostream& operator<<(std::ostream& os, const Token& tok);
    token type;
    std::string val;
    size_t line;
};


/*
 * Tokenizer Class
 */
class Tokenizer {

private:
    const char * input;
    size_t input_length;
    size_t pos;
    char current_char;
    size_t line;
    std::string buffer;
    std::vector<Token> tokens;

    static bool contains(char c, std::vector<char>& vec);
    static bool contains(const std::vector<std::string>& vec, const std::string& value);
    static bool contains(const std::vector<Keyword>& vec, const std::string& value);
    static token get_token_type(const std::vector<Keyword>& vec, const std::string& value);
    [[nodiscard]] char peek(int ahead = 1) const;
    void next_char(int chars = 1);
    void flush_buffer();
	void skip_whitespace();
    std::string look_ahead(int chars);

	static bool is_space(const char& ch);
	[[nodiscard]] bool is_string() const;
    bool is_keyword_or_num() const;

    void get_line_continuation();
    void get_bitwise_operator();
    bool is_callback_end();
    bool is_callback_start();
    static bool is_hexadecimal(const std::string& str);
    static bool is_binary(const std::string& str);
    void get_comma();
    void get_linebreak();
    void get_comment();
	void get_invalid();
    void get_comparison();
	void get_string();
    void get_math();
    void get_parenth();
    void get_assignment();
    void get_arrow();
    void get_keyword_or_num();

public:
    explicit Tokenizer(const char* &input);
    ~Tokenizer() = default;
    void tokenize();
};
