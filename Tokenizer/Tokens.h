//
// Created by Mathias Vatter on 20.08.23.
//

#pragma once
#include <iostream>
#include <utility>

/// defines the token names and the string that represents them while debugging
#define ENUM_LIST(XX) \
	XX(INVALID, "invalid") \
	XX(END_TOKEN, "end_of_file") \
	XX(LINEBRK, "linebreak") \
	XX(TYPE, "type") \
	XX(INT, "int") \
	XX(FLOAT, "float") \
	XX(HEXADECIMAL, "hexadecimal") \
	XX(BINARY, "binary") \
	XX(COMMENT, "comment") \
    XX(STRING, "string") \
    XX(COMMA, "comma sep") \
    XX(DOT, "dot") \
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
	XX(STRING_OP, "add_string")        \
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
    XX(NOT_EQUAL, "not_equal") \
    XX(COMPARISON, "comparison_operator") \
    XX(BOOL_AND, "bool_and") \
    XX(BOOL_OR, "bool_or") \
    XX(BOOL_NOT, "bool_not") \
    XX(FUNCTION, "function") \
    XX(CALL, "func_call") \
    XX(OVERRIDE, "override") \
    XX(FOR, "for") \
    XX(WHILE, "while") \
    XX(IF, "if") \
    XX(SELECT, "select") \
    XX(LIST, "list") \
    XX(CONST, "const") \
    XX(FAMILY, "family") \
    XX(STRUCT, "struct") \
    XX(TASKFUNC, "taskfunc") \
    XX(MACRO, "macro") \
    XX(END_FUNCTION, "end function") \
    XX(END_FOR, "end for") \
    XX(END_WHILE, "end while") \
    XX(END_IF, "end if") \
    XX(END_SELECT, "end select") \
    XX(END_CONST, "end constblock") \
    XX(END_LIST, "end list") \
    XX(END_FAMILY, "end family") \
    XX(END_STRUCT, "end struct") \
    XX(END_TASKFUNC, "end taskfunc") \
    XX(END_MACRO, "end macro") \
    XX(TO, "to") \
    XX(STEP, "step") \
    XX(DOWNTO, "downto") \
    XX(ELSE, "else") \
    XX(CASE, "select_case")   \
    XX(IMPORT, "import") \
    XX(AS, "import_as") \
    XX(DECLARE, "declare") \
    XX(LOCAL, "local") \
    XX(GLOBAL, "global") \
    XX(READ, "read") \
    XX(DEFINE, "define") \
    XX(POLYPHONIC, "polyphonic") \
    XX(LINE_CONTINUE, "line_continue") \
    XX(ON, "on") \
    XX(ITERATE_MACRO, "iterate_macro") \
    XX(LITERATE_MACRO, "literate_macro") \
    XX(SET_CONDITION, "set_condition") \
    XX(RESET_CONDITION, "reset_condition") \
    XX(USE_CODE_IF, "use_code_if") \
    XX(USE_CODE_IF_NOT, "use_code_if_not") \
    XX(END_USE_CODE, "end_use_code") \
    XX(START_INC, "start_inc") \
    XX(END_INC, "end_inc") \
    XX(UI_CONTROL, "ui_control")


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
	inline Keyword(token type, std::string keyword) : type(type), value(std::move(keyword)) {};
	token type;
	std::string value;
};

inline std::vector<char> BINARY_OPERATORS = {'-', '+', '/', '*', '&'};
inline std::vector<char> PARENTH = {'(',')', '[', ']'};
inline std::vector<char> VAR_IDENT = {'$', '~', '@'};
inline std::vector<char> ARRAY_IDENT = {'%', '?', '!'}; //int, real, string
inline std::vector<Keyword> TYPES = {{INT, "$"}, {FLOAT, "~"}, {STRING, "@"}, {INT, "%"}, {FLOAT, "?"}, {STRING, "!"}};
inline std::vector<char> COMMENT_START = {'{', '/'};
inline std::vector<char> COMPARISON_OPERATORS_START = {'<', '>', '=', '#'};
inline std::vector<std::string> UI_CONTROLS = {"ui_label", "ui_button", "ui_switch", "ui_slider", "ui_menu",
										   "ui_value_edit", "ui_waveform", "ui_wavetable",
										   "ui_knob", "ui_table", "ui_xy",
										   "ui_text_edit", "ui_level_meter", "ui_file_selector",
										   "ui_panel", "ui_mouse_area"};
inline std::vector<Keyword> DECLARATION_SYNTAX = {{DECLARE, "declare"}, {DEFINE, "define"}, {CONST, "const"}, {POLYPHONIC, "polyphonic"},
                                                  {READ, "read"}, {LOCAL, "local"}, {GLOBAL, "global"}};
inline std::vector<Keyword> PREPROCESSOR_SYNTAX = {{IMPORT, "import"}, {AS, "as"}, {ON, "on"},
												   {ITERATE_MACRO, "iterate_macro"}, {LITERATE_MACRO, "literate_macro"},
													{START_INC, "START_INC"}, {END_INC, "END_INC"}, {SET_CONDITION, "SET_CONDITION"}, {RESET_CONDITION, "RESET_CONDITION"},
                                                   {USE_CODE_IF, "USE_CODE_IF"}, {USE_CODE_IF_NOT, "USE_CODE_IF_NOT"}, {END_USE_CODE, "END_USE_CODE"}};
inline std::vector<Keyword> STATEMENT_SYNTAX = {{TO, "to"}, {DOWNTO, "downto"}, {STEP, "step"}, {ELSE, "else"}, {CASE, "case"}};
inline std::vector<Keyword> FUNCTION_SYNTAX = {{OVERRIDE, "override"}, {CALL, "call"}};
// control statements that also have an end
inline std::vector<Keyword> END_STATEMENTS = {{END_FUNCTION, "end function"}, {END_FOR, "end for"}, {END_WHILE, "end while"},
											  {END_IF,       "end if"}, {END_SELECT, "end select"}, {END_CONST, "end const"},
                                              {END_LIST, "end list"}, {END_FAMILY, "end family"}, {END_STRUCT, "end struct"},
											  {END_MACRO,    "end macro"}, {END_TASKFUNC, "end taskfunc"}};
inline std::vector<Keyword> STATEMENTS = {{FUNCTION, "function"}, {FOR, "for"}, {WHILE, "while"}, {IF, "if"},
                                          {SELECT, "select"}, {CONST, "const"}, {LIST, "list"}, {FAMILY, "family"},
                                          {STRUCT, "struct"}, {MACRO,    "macro"}, {TASKFUNC, "taskfunc"}};
inline std::vector<std::string> CALLBACKS = {"init", "note", "release", "midi_in", "controller",
											 "rpn", "nrpn", "ui_update", "_pgs_changed", "pgs_changed",
											 "poly_at", "listener", "async_complete", "persistence_changed", "ui_control"};
inline std::vector<Keyword> BITWISE_OPERATORS = {{BIT_AND, ".and."}, {BIT_OR, ".or."}, {BIT_NOT, ".not."}, {BIT_XOR, ".xor."}};
inline std::vector<Keyword> BOOL_OPERATORS = {{BOOL_AND, "and"}, {BOOL_OR, "or"}, {BOOL_NOT, "not"}};
inline std::vector<Keyword> MATH_OPERATORS = {{SUB, "-"}, {ADD, "+"}, {DIV, "/"}, {MULT, "*"}, {MODULO, "mod"}};
inline std::vector<Keyword> UNARY_OPERATORS = {{SUB, "-"}, {BIT_NOT, ".not."}, {BOOL_NOT, "not"}};
inline std::vector<Keyword> COMPARISON_OPERATORS = {{LESS_THAN, "<"}, {GREATER_THAN, ">"}, {EQUAL, "="}, {LESS_EQUAL, "<="}, {GREATER_EQUAL, ">="}, {NOT_EQUAL, "#"}};
inline std::vector<Keyword> STRING_OPERATOR = {{STRING_OP, "&"}};
inline const std::vector<Keyword> ALL_OPERATORS = []{
    std::vector<Keyword> ops = BITWISE_OPERATORS;
    ops.insert(ops.end(), MATH_OPERATORS.begin(), MATH_OPERATORS.end());
    ops.insert(ops.end(), BOOL_OPERATORS.begin(), BOOL_OPERATORS.end());
    return ops;
}();

