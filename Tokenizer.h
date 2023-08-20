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

/// combines the token type and the actual Keyword that should be searched for by lexer
struct Keyword {
    inline Keyword(token type, std::string keyword) : type(type), value(keyword) {};

    token type;
    std::string value;
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

    std::vector<char> MATH = {'-','+', '/', '*'};
    std::vector<char> PARENTH = {'(',')', '[', ']'};
    std::vector<char> VAR_IDENT = {'$', '%', '~', '?', '@', '!'};
    std::vector<char> COMMENT_START = {'{', '/'};
    std::vector<char> COMPARISON_START = {'<', '>', '='};

    std::vector<Keyword> UI_CONTROLS = {{UI_LABEL, "ui_label"}, {UI_BUTTON, "ui_button"}, {UI_SWITCH, "ui_switch"}, {UI_SLIDER, "ui_slider"}, {UI_MENU, "ui_menu"},
                                        {UI_VALUE_EDIT, "ui_value_edit"}, {UI_WAVEFORM, "ui_waveform"}, {UI_WAVETABLE, "ui_wavetable"},
                                        {UI_KNOB, "ui_knob"}, {UI_TABLE, "ui_table"}, {UI_XY, "ui_xy"},
                                        {UI_TEXT_EDIT, "ui_text_edit"}, {UI_LEVEL_METER, "ui_level_meter"}, {UI_FILE_SELECTOR, "ui_file_selector"},
                                        {UI_PANEL, "ui_panel"}, {UI_MOUSE_AREA, "ui_mouse_area"}};
    std::vector<Keyword> STATEMENT_SYNTAX = {{TO, "to"}, {DOWNTO, "downto"}, {ELSE, "else"}, {CASE, "case"}, {AS, "as"}};
    // control statements that also have an end
    std::vector<Keyword> END_STATEMENTS = {{END_FUNCTION, "end function"}, {END_FOR, "end for"}, {END_WHILE, "end while"},
                                           {END_IF,       "end if"}, {END_SELECT, "end select"}, {END_CONSTBLOCK, "end const"}, {END_LIST, "end list"},
                                           {END_MACRO,    "end macro"}, {END_TASKFUNC, "end taskfunc"}};
    std::vector<Keyword> STATEMENTS = {{FUNCTION, "function"}, {FOR, "for"}, {WHILE, "while"},
                                       {IF,       "if"}, {SELECT, "select"}, {CONSTBLOCK, "const"}, {LIST, "list"},
                                       {MACRO,    "macro"}, {TASKFUNC, "taskfunc"}};
    std::vector<std::string> CALLBACKS = {"init", "note", "release", "midi_in", "controller",
                                          "rpn", "nrpn", "ui_update", "_pgs_changed", "pgs_changed",
                                          "poly_at", "listener", "async_complete", "persistence_changed", "ui_control"};
    std::vector<Keyword> BITWISE_OPERATORS = {{BIT_AND, ".and."}, {BIT_OR, ".or."}, {BIT_NOT, ".not."}, {BIT_XOR, ".xor."}};
    std::vector<Keyword> BOOL_OPERATORS = {{BOOL_AND, "and"}, {BOOL_OR, "or"}, {BOOL_NOT, "not"}};

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
    explicit Tokenizer(const char* &input);
    ~Tokenizer() = default;
    void tokenize();
};
