//
// Created by Mathias Vatter on 03.06.23.
//

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <map>

#include "../Result.h"
#include "Tokens.h"


/*
 * Token struct that gets line numbers, the token type and its value
 */
struct Token {
    Token() : type(token::INVALID), val(""), line(0), file("") {}
    Token(token type, std::string val, size_t line, std::string &file);
    friend std::ostream& operator<<(std::ostream& os, const Token& tok);
    token type;
    std::string val;
    size_t line;
    std::string file;
};

/*
 * helper function to search vectors for chars, std::String and Keyword obj
 */
inline static bool contains(std::vector<char> &vec, char c) {
    return std::any_of(vec.begin(), vec.end(), [&](const auto& ch) {return ch == c;});
};
inline static bool contains(const std::vector<std::string>& vec, const std::string& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
};
inline static bool contains(const std::vector<Keyword>& vec, const std::string& value) {
    return std::find_if(vec.begin(), vec.end(), [&value](const Keyword& kw) {
        return kw.value == value;
    }) != vec.end();
};

inline static long count_char(const std::string& str, char c) {
	return std::count(str.begin(), str.end(), c);
}

/*
 * Tokenizer Class
 */
class Tokenizer {

private:
    std::string input;
    std::string file;
    size_t input_length;
    size_t pos;
    char current_char;
    size_t line;
    std::string buffer;
    std::vector<Token> tokens;

    static token get_token_type(const std::vector<Keyword>& vec, const std::string& value);
    [[nodiscard]] char peek(int ahead = 1) const;
    void consume(int chars = 1);
    void flush_buffer();
	void skip_whitespace();

    static std::string read_file(const std::string& filename);

	static bool is_space(const char& ch);
	[[nodiscard]] bool is_string() const;
    bool is_keyword_or_num() const;

    void get_line_continuation();
    /// removes linebrk if there was a line_continuation before. Needs to be inserted right after linbrk isnert
    void fix_line_continuation();
    void get_bitwise_operator();
    bool is_callback_end();
    bool is_callback_start();
    static bool is_hexadecimal(const std::string& str);
    static bool is_binary(const std::string& str);
    void get_comma();
    void get_linebreak();
    void get_comment();
	void get_invalid();
    void get_comparison_operators();
	void get_string();
    void get_binary_operators();
    void get_parenth();
    void get_assignment();
    void get_arrow();
    void get_keyword_or_num();

public:
    explicit Tokenizer(std::string file);
    ~Tokenizer() = default;
    std::vector<Token> tokenize();
};
