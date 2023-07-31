//
// Created by Mathias Vatter on 03.06.23.
//

#ifndef COMPILER_LEXER_H
#define COMPILER_LEXER_H

#include <string>
#include <vector>

/*
 * Callbacks
 * Variables
 * Operator
 * Control Statement
 *
 */

enum token {
    INVALID,
    INT,
    FLOAT,
    STRING,
    ASSIGN,
    CALLBACK,
    PREPROC,
    COMMENT // {} /* */
};

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
    char prev_char;
    char current_char;
    size_t line;
    std::string buffer;
    std::vector<Token> tokens;
public:

    explicit Lexer(const std::string& input);
    ~Lexer() = default;

    void flush_buffer();
    void next_char(int chars = 1);
    std::string look_ahead(int chars);
    Token next_token();
    Token get_comment();
    Token get_callback();
};


#endif //COMPILER_LEXER_H
