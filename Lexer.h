//
// Created by Mathias Vatter on 03.06.23.
//

#ifndef COMPILER_LEXER_H
#define COMPILER_LEXER_H

#include <string>
#include <vector>
#include <iostream>
#include <map>

/*
 * Callbacks
 * Variables
 * Operator
 * Control Statement
 *
 */

#define ENUM_LIST(XX) \
	XX(INVALID, "invalid") \
	XX(INT, "int") \
	XX(FLOAT, "float") \
	XX(COMMENT, "comment") \
    XX(STRING, "string") \
	XX(KEYWORD, "keyword") \
	XX(CALLBACK, "begin callback") \
	XX(END_CALLBACK, "end callback") \
	XX(ASSIGN, ":=")     \
	XX(ARROW, "->")     \
    XX(EQUAL, "=") \
	XX(SUB, "-") \
	XX(ADD, "+") \
    XX(DIV, "/")      \
	XX(MULT, "*")        \
    XX(OPEN_PARENTH, "(")\
    XX(CLOSED_PARENTH, ")")       \
    XX(OPEN_BRACKET, "[")\
    XX(CLOSED_BRACKET, "]") \
    XX(OPEN_CURLY, "{") \
    XX(CLOSED_CURLY, "}")\
    XX(GT, ">") \
    XX(s, "invalsid")


#define ENUM(name, str) name,
enum token {
	ENUM_LIST(ENUM)
};
#undef ENUM

extern const char *tokenStrings[];

std::ostream& operator<<(std::ostream& os, const token& tok);

/*
 * LEXER CLASS
 */
class Lexer {
public:
    struct Token {
        Token(token type, const std::string &val, size_t line);
        friend std::ostream& operator<<(std::ostream& os, const Token& tok);

        token type;
        std::string val;
        size_t line;
    };
private:
    const char * input;
    size_t input_length;
    size_t pos;
    char current_char;
    size_t line;
    std::string buffer;
    std::vector<Token> tokens;

	static bool is_space(const char& ch);
    void flush_buffer();
    void next_char(int chars = 1);
	void skip_whitespace();
    std::string look_ahead(int chars);

	bool is_comment();
    void get_comment();
	bool is_callback();
    void get_callback();
	void get_invalid();
	bool is_string() const;
	void get_string();
    bool is_math() const;
    void get_math();
    bool is_bracket() const;
    void get_bracket();
    bool is_assignment();
    void get_assignment();
    bool is_arrow();
    void get_arrow();
    bool is_text_or_num();
    void get_text_or_num();
    void get_literal();
    bool is_var_identifier(char c);
    std::vector<char> MATH = {'-','+', '/', '*'};
    std::vector<char> PARENTH = {'(',')', '[', ']', '{', '}'};
    std::vector<char> VAR_IDENT = {'$', '%', '~', '?', '@', '!'};

public:
    explicit Lexer(const char* &input);
    ~Lexer() = default;


    void next_token();
};


#endif //COMPILER_LEXER_H
