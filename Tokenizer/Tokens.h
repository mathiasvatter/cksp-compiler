//
// Created by Mathias Vatter on 20.08.23.
//

#pragma once
#include <iostream>
//#include <utility>
#include <unordered_map>
#include <set>

#include "version.h"

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
    XX(IN, "in") \
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

inline std::set<char> BINARY_OPERATORS = {'-', '+', '/', '*', '&'};
inline std::set<char> PARENTH = {'(',')', '[', ']'};
inline std::set<char> VAR_IDENT = {'$', '~', '@'};
inline std::set<char> ARRAY_IDENT = {'%', '?', '!'}; //int, real, string
inline std::unordered_map<std::string, token> TYPES = {{"$", INT}, {"~", FLOAT}, {"@", STRING}, {"%", INT}, {"?", FLOAT}, {"!", STRING}};
inline std::set<char> COMMENT_START = {'{', '/'};
inline std::set<char> COMPARISON_OPERATORS_START = {'<', '>', '=', '#'};
inline std::set<std::string> UI_CONTROLS = {"ui_label", "ui_button", "ui_switch", "ui_slider", "ui_menu",
										   "ui_value_edit", "ui_waveform", "ui_wavetable",
										   "ui_knob", "ui_table", "ui_xy",
										   "ui_text_edit", "ui_level_meter", "ui_file_selector",
										   "ui_panel", "ui_mouse_area"};
inline std::unordered_map<std::string, token> DECLARATION_SYNTAX = {{"declare", DECLARE}, {"define", DEFINE}, {"const", CONST}, {"polyphonic", POLYPHONIC},
                                                  {"read", READ}, {"local", LOCAL}, {"global", GLOBAL}};
inline std::unordered_map<std::string, token> PREPROCESSOR_SYNTAX = {{"import", IMPORT}, {"as", AS}, {"on", ON},
												   {"iterate_macro", ITERATE_MACRO}, {"literate_macro", LITERATE_MACRO},
													{"START_INC", START_INC}, {"END_INC", END_INC}, {"SET_CONDITION", SET_CONDITION}, {"RESET_CONDITION", RESET_CONDITION},
                                                   {"USE_CODE_IF", USE_CODE_IF}, {"USE_CODE_IF_NOT", USE_CODE_IF_NOT}, {"END_USE_CODE", END_USE_CODE}};
inline std::unordered_map<std::string, token> STATEMENT_SYNTAX = {{"to", TO}, {"downto", DOWNTO}, {"step", STEP}, {"else", ELSE}, {"case", CASE}, {"in", IN}};
inline std::unordered_map<std::string, token> FUNCTION_SYNTAX = {{"override", OVERRIDE}, {"call", CALL}};
// control statements that also have an end
inline std::unordered_map<std::string, token> END_STATEMENTS = {{"end function", END_FUNCTION}, {"end for", END_FOR}, {"end while", END_WHILE},
											  {"end if", END_IF}, {"end select", END_SELECT}, {"end const", END_CONST},
                                              {"end list", END_LIST}, {"end family", END_FAMILY}, {"end struct", END_STRUCT},
											  {"end macro", END_MACRO}, {"end taskfunc", END_TASKFUNC}};
inline std::unordered_map<std::string, token> STATEMENTS = {{"function", FUNCTION}, {"for", FOR}, {"while", WHILE}, {"if", IF},
                                          {"select", SELECT}, {"const", CONST}, {"list", LIST}, {"family", FAMILY},
                                          {"struct", STRUCT}, {"macro", MACRO}, {"taskfunc", TASKFUNC}};
inline std::set<std::string> CALLBACKS = {"init", "note", "release", "midi_in", "controller",
											 "rpn", "nrpn", "ui_update", "_pgs_changed", "pgs_changed",
											 "poly_at", "listener", "async_complete", "persistence_changed", "ui_control"};
inline std::set<std::string> RESTRICTED_CALLBACKS = {"init", "persistence_changed", "ui_control", "pgs_changed", "async_complete"};
inline std::unordered_map<std::string, token> BITWISE_OPERATORS = {{".and.", BIT_AND}, {".or.", BIT_OR}, {".not.", BIT_NOT}, {".xor.", BIT_XOR}};
//inline std::vector<Keyword> BITWISE_OPERATORS = {{BIT_AND, ".and."}, {BIT_OR, ".or."}, {BIT_NOT, ".not."}, {BIT_XOR, ".xor."}};
inline std::unordered_map<std::string, token> BOOL_OPERATORS = {{"and", BOOL_AND}, {"or", BOOL_OR}, {"not", BOOL_NOT}};
//inline std::vector<Keyword> BOOL_OPERATORS = {{BOOL_AND, "and"}, {BOOL_OR, "or"}, {BOOL_NOT, "not"}};
inline std::unordered_map<std::string, token> MATH_OPERATORS = {{"-", SUB}, {"+", ADD}, {"/", DIV}, {"*", MULT}, {"mod", MODULO}};
//inline std::vector<Keyword> MATH_OPERATORS = {{SUB, "-"}, {ADD, "+"}, {DIV, "/"}, {MULT, "*"}, {MODULO, "mod"}};
inline std::unordered_map<std::string, token> UNARY_OPERATORS = {{"-", SUB}, {".not.", BIT_NOT}, {"not", BOOL_NOT}};
//inline std::vector<Keyword> UNARY_OPERATORS = {{SUB, "-"}, {BIT_NOT, ".not."}, {BOOL_NOT, "not"}};
inline std::unordered_map<std::string, token> COMPARISON_OPERATORS = {{"<", LESS_THAN}, {">", GREATER_THAN}, {"=", EQUAL}, {"<=", LESS_EQUAL}, {">=", GREATER_EQUAL}, {"#", NOT_EQUAL}};
//inline std::vector<Keyword> COMPARISON_OPERATORS = {{LESS_THAN, "<"}, {GREATER_THAN, ">"}, {EQUAL, "="}, {LESS_EQUAL, "<="}, {GREATER_EQUAL, ">="}, {NOT_EQUAL, "#"}};
inline std::unordered_map<std::string, token> STRING_OPERATOR = {{"&", STRING_OP}};
//inline std::vector<Keyword> STRING_OPERATOR = {{STRING_OP, "&"}};
inline const std::unordered_map<std::string, token> ALL_OPERATORS = []{
    std::unordered_map<std::string, token> ops = BITWISE_OPERATORS;
    ops.insert(MATH_OPERATORS.begin(), MATH_OPERATORS.end());
    ops.insert(BOOL_OPERATORS.begin(), BOOL_OPERATORS.end());
    return ops;
}();

