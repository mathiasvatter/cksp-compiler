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
	XX(LINEBRK, "linebreak") \
	XX(INT, "int") \
	XX(FLOAT, "float") \
	XX(HEXADECIMAL, "hexadecimal") \
	XX(BINARY, "binary") \
	XX(COMMENT, "comment") \
    XX(STRING, "string") \
    XX(COMMA, "comma sep") \
	XX(KEYWORD, "keyword") \
	XX(BEGIN_CALLBACK, "begin callback") \
	XX(END_CALLBACK, "end callback") \
	XX(ASSIGN, ":=")     \
	XX(ARROW, "->")     \
	XX(SUB, "-") \
	XX(ADD, "+") \
    XX(DIV, "/")      \
	XX(MULT, "*")        \
	XX(MODULO, "modulo")        \
	XX(BIT_AND, "bit_and")        \
	XX(BIT_OR, "bit_or")        \
	XX(BIT_XOR, "bit_xor")        \
	XX(BIT_NOT, "bit_not")        \
    XX(OPEN_PARENTH, "(")\
    XX(CLOSED_PARENTH, ")")       \
    XX(OPEN_BRACKET, "[")\
    XX(CLOSED_BRACKET, "]") \
    XX(GREATER_THAN, ">") \
    XX(LESS_THAN, ">") \
    XX(GREATER_EQUAL, ">=") \
    XX(LESS_EQUAL, "<=") \
    XX(EQUAL, "=") \
    XX(BOOL_AND, "bool_and") \
    XX(BOOL_OR, "bool_or") \
    XX(BOOL_NOT, "bool_not") \
    XX(FUNCTION, "function") \
    XX(FOR, "for") \
    XX(WHILE, "while") \
    XX(IF, "if") \
    XX(SELECT, "select") \
    XX(CONSTBLOCK, "constblock") \
    XX(LIST, "list") \
    XX(TASKFUNC, "taskfunc") \
    XX(MACRO, "macro") \
    XX(END_FUNCTION, "end function") \
    XX(END_FOR, "end for") \
    XX(END_WHILE, "end while") \
    XX(END_IF, "end if") \
    XX(END_SELECT, "end select") \
    XX(END_CONSTBLOCK, "end constblock") \
    XX(END_LIST, "end list") \
    XX(END_TASKFUNC, "end taskfunc") \
    XX(END_MACRO, "end macro") \
    XX(LINE_CONTINUE, "line_continue")


#define ENUM(name, str) name,
enum token {
	ENUM_LIST(ENUM)
};
#undef ENUM

extern const char *tokenStrings[];

std::ostream& operator<<(std::ostream& os, const token& tok);

struct Token {
    Token(token type, const std::string &val, size_t line);
    friend std::ostream& operator<<(std::ostream& os, const Token& tok);

    token type;
    std::string val;
    size_t line;
};

/// combines the token type and the actual keyword that should be searched for by lexer
struct keyword {
    inline keyword(token type, std::string keyword) : type(type), value(keyword) {};

    token type;
    std::string value;
};

/*
 * LEXER CLASS
 */
class Lexer {

private:
    const char * input;
    size_t input_length;
    size_t pos;
    char current_char;
    size_t line;
    std::string buffer;
    std::vector<Token> tokens;

    std::vector<char> MATH = {'-','+', '/', '*'};
    std::vector<char> PARENTH = {'(',')', '[', ']'};
    std::vector<char> VAR_IDENT = {'$', '%', '~', '?', '@', '!'};
    std::vector<char> COMMENT_START = {'{', '/'};
    std::vector<char> COMPARISON_START = {'<', '>', '='};
    // control statements that also have an end
    std::vector<keyword> END_STATEMENTS = {{END_FUNCTION, "end function"}, {END_FOR, "end for"}, {END_WHILE, "end while"},
                                           {END_IF, "end if"}, {END_SELECT, "end select"}, {END_CONSTBLOCK, "end const"}, {END_LIST, "end list"},
                                           {END_MACRO, "end macro"}, {END_TASKFUNC, "end taskfunc"}};
    std::vector<keyword> STATEMENTS = {{FUNCTION, "function"}, {FOR, "for"}, {WHILE, "while"},
                                       {IF, "if"}, {SELECT, "select"}, {CONSTBLOCK, "const"}, {LIST, "list"},
                                       {MACRO, "macro"}, {TASKFUNC, "taskfunc"}};
    std::vector<std::string> CALLBACKS = {"init", "note", "release", "midi_in", "controller",
                                          "rpn", "nrpn", "ui_update", "_pgs_changed", "pgs_changed",
                                          "poly_at", "listener", "async_complete", "persistence_changed", "ui_control"};
    std::vector<keyword> BITWISE_OPERATORS = {{BIT_AND, ".and."}, {BIT_OR, ".or."}, {BIT_NOT, ".not."}, {BIT_XOR, ".xor."}};
    std::vector<keyword> BOOL_OPERATORS = {{BOOL_AND,"and"}, {BOOL_OR, "or"}, {BOOL_NOT, "not"}};

    static bool contains(char c, std::vector<char>& vec);
    static bool contains(const std::vector<std::string>& vec, const std::string& value);
    static bool contains(const std::vector<keyword>& vec, const std::string& value);
    static token get_token_type(const std::vector<keyword>& vec, const std::string& value);
    [[nodiscard]] char peek(int ahead = 1) const;
    void next_char(int chars = 1);
    void flush_buffer();
	void skip_whitespace();
    std::string look_ahead(int chars);

	static bool is_space(const char& ch);
	[[nodiscard]] bool is_string() const;
    bool is_keyword_or_num();

    void get_line_continuation();
    void get_bitwise_operator();
    bool is_callback_end();
    bool is_callback_start();
    bool is_hexadecimal(const std::string& str);
    bool is_binary(const std::string& str);
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
    explicit Lexer(const char* &input);
    ~Lexer() = default;
    void tokenize();
};


#endif //COMPILER_LEXER_H
