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
	XX(CALLBACK, "callback") \
	XX(END_CALLBACK, "end callback") \
	XX(KEYWORDS, "keywords:") \
	XX(ASSIGN, ":=") \
	XX(EQUAL, "=") \
	XX(SUB, "-") \
	XX(ADD, "+") \
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
    std::string input;
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
	bool is_declaration();
	void get_declaration();
	bool is_num() const;
	void get_num();

public:

    explicit Lexer(const std::string& input);
    ~Lexer() = default;

    void next_token();
};


#endif //COMPILER_LEXER_H
