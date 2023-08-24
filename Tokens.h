//
// Created by Mathias Vatter on 20.08.23.
//

#ifndef COMPILER_TOKENS_H
#define COMPILER_TOKENS_H

/// defines the token names and the string that represents them while debugging
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
	XX(KEYWORD, "Keyword") \
	XX(BEGIN_CALLBACK, "begin callback") \
	XX(END_CALLBACK, "end callback") \
	XX(ASSIGN, "assignment")     \
	XX(ARROW, "arrow")     \
	XX(SUB, "sub") \
	XX(ADD, "add") \
    XX(DIV, "div")      \
	XX(MULT, "mult")        \
	XX(MODULO, "modulo")        \
	XX(BIT_AND, "bit_and")        \
	XX(BIT_OR, "bit_or")        \
	XX(BIT_XOR, "bit_xor")        \
	XX(BIT_NOT, "bit_not")        \
    XX(OPEN_PARENTH, "open_parenth")\
    XX(CLOSED_PARENTH, "closed_parenth")       \
    XX(OPEN_BRACKET, "open_bracket")\
    XX(CLOSED_BRACKET, "closed_bracked") \
    XX(GREATER_THAN, "greater than") \
    XX(LESS_THAN, "less than") \
    XX(GREATER_EQUAL, "greater equal") \
    XX(LESS_EQUAL, "less equal") \
    XX(EQUAL, "equal") \
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
    XX(TO, "to") \
    XX(DOWNTO, "downto") \
    XX(ELSE, "else") \
    XX(CASE, "select_case") \
    XX(AS, "as") \
    XX(LINE_CONTINUE, "line_continue") \
    XX(UI_LABEL, "") \
    XX(UI_BUTTON, "") \
    XX(UI_SWITCH, "") \
    XX(UI_SLIDER, "") \
    XX(UI_MENU, "") \
    XX(UI_VALUE_EDIT, "") \
    XX(UI_WAVEFORM, "") \
    XX(UI_WAVETABLE, "") \
    XX(UI_KNOB, "") \
    XX(UI_TABLE, "") \
    XX(UI_XY, "") \
    XX(UI_TEXT_EDIT, "") \
    XX(UI_LEVEL_METER, "") \
    XX(UI_FILE_SELECTOR, "") \
    XX(UI_PANEL, "") \
    XX(UI_MOUSE_AREA, "")



#define ENUM(name, str) name,
enum token {
    ENUM_LIST(ENUM)
};
#undef ENUM


#define STRING(name, str) str,
inline const char *tokenStrings[] = {
        ENUM_LIST(STRING)
};
#undef STRING
//inline const char *tokenStrings[];

/// overwrite the << operator to make debugging easier
inline std::ostream &operator<<(std::ostream &os, const token &tok) {
    os << tokenStrings[tok];
    return os;
}

/// combines the token type and the actual Keyword that should be searched for by lexer
struct Keyword {
	inline Keyword(token type, std::string keyword) : type(type), value(keyword) {};
	token type;
	std::string value;
};

inline std::vector<char> MATH = {'-','+', '/', '*'};
inline std::vector<char> PARENTH = {'(',')', '[', ']'};
inline std::vector<char> VAR_IDENT = {'$', '%', '~', '?', '@', '!'};
inline std::vector<char> COMMENT_START = {'{', '/'};
inline std::vector<char> COMPARISON_START = {'<', '>', '='};
inline std::vector<Keyword> UI_CONTROLS = {{UI_LABEL, "ui_label"}, {UI_BUTTON, "ui_button"}, {UI_SWITCH, "ui_switch"}, {UI_SLIDER, "ui_slider"}, {UI_MENU, "ui_menu"},
										   {UI_VALUE_EDIT, "ui_value_edit"}, {UI_WAVEFORM, "ui_waveform"}, {UI_WAVETABLE, "ui_wavetable"},
										   {UI_KNOB, "ui_knob"}, {UI_TABLE, "ui_table"}, {UI_XY, "ui_xy"},
										   {UI_TEXT_EDIT, "ui_text_edit"}, {UI_LEVEL_METER, "ui_level_meter"}, {UI_FILE_SELECTOR, "ui_file_selector"},
										   {UI_PANEL, "ui_panel"}, {UI_MOUSE_AREA, "ui_mouse_area"}};
inline std::vector<Keyword> STATEMENT_SYNTAX = {{TO, "to"}, {DOWNTO, "downto"}, {ELSE, "else"}, {CASE, "case"}, {AS, "as"}};
// control statements that also have an end
inline std::vector<Keyword> END_STATEMENTS = {{END_FUNCTION, "end function"}, {END_FOR, "end for"}, {END_WHILE, "end while"},
											  {END_IF,       "end if"}, {END_SELECT, "end select"}, {END_CONSTBLOCK, "end const"}, {END_LIST, "end list"},
											  {END_MACRO,    "end macro"}, {END_TASKFUNC, "end taskfunc"}};
inline std::vector<Keyword> STATEMENTS = {{FUNCTION, "function"}, {FOR, "for"}, {WHILE, "while"},
										  {IF,       "if"}, {SELECT, "select"}, {CONSTBLOCK, "const"}, {LIST, "list"},
										  {MACRO,    "macro"}, {TASKFUNC, "taskfunc"}};
inline std::vector<std::string> CALLBACKS = {"init", "note", "release", "midi_in", "controller",
											 "rpn", "nrpn", "ui_update", "_pgs_changed", "pgs_changed",
											 "poly_at", "listener", "async_complete", "persistence_changed", "ui_control"};
inline std::vector<Keyword> BITWISE_OPERATORS = {{BIT_AND, ".and."}, {BIT_OR, ".or."}, {BIT_NOT, ".not."}, {BIT_XOR, ".xor."}};
inline std::vector<Keyword> BOOL_OPERATORS = {{BOOL_AND, "and"}, {BOOL_OR, "or"}, {BOOL_NOT, "not"}};



#endif //COMPILER_TOKENS_H
