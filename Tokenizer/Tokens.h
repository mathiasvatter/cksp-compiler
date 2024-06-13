//
// Created by Mathias Vatter on 20.08.23.
//

#pragma once
#include <iostream>
//#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <vector>
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
	XX(SUB, "-") \
	XX(ADD, "+") \
    XX(DIV, "/")      \
	XX(MULT, "*")        \
	XX(MODULO, "mod")        \
	XX(STRING_OP, "&")        \
	XX(BIT_AND, ".and.")        \
	XX(BIT_OR, ".or.")        \
	XX(BIT_XOR, "bit_xor")        \
	XX(BIT_NOT, "bit_not")        \
    XX(OPEN_PARENTH, "open_parenth")\
    XX(CLOSED_PARENTH, "closed_parenth")       \
    XX(OPEN_BRACKET, "open_bracket")\
    XX(CLOSED_BRACKET, "closed_bracket") \
    XX(OPEN_CURLY, "closed_curly") \
    XX(CLOSED_CURLY, "closed_curly") \
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
    XX(BOOL_XOR, "bool_xor") \
    XX(FUNCTION, "function") \
    XX(CALL, "func_call") \
    XX(OVERRIDE, "override") \
    XX(FOR, "for") \
    XX(WHILE, "while") \
    XX(IF, "if") \
    XX(SELECT, "select") \
    XX(DEFAULT, "default") \
    XX(LIST, "list") \
    XX(CONST, "const") \
    XX(FAMILY, "family") \
    XX(STRUCT, "struct") \
    XX(TASKFUNC, "taskfunc") \
    XX(COMPONENT, "component") \
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
    XX(END_COMPONENT, "end component") \
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
    XX(PERS, "pers") \
    XX(INSTPERS, "instpers") \
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
    XX(UI_CONTROL, "ui_control")\
    XX(RETURN, "return")\
    XX(FALSE, "false")\
    XX(TRUE, "true")\
    XX(NEW, "new")\
    XX(DELETE, "delete")\
    XX(NIL, "nil")\
	XX(PRAGMA, "pragma")

#define ENUM(name, str) name,
enum class token {
    ENUM_LIST(ENUM)
};
#undef ENUM


#define STRING(name, str) str,
inline const char *tokenStrings[] = {
        ENUM_LIST(STRING)
};
#undef STRING

/// overwrite the << operator to make debugging easier
inline std::ostream &operator<<(std::ostream &os, const token &tok) {
    os << tokenStrings[static_cast<int>(tok)];
    return os;
}

/// combines the token type and the actual Keyword that should be searched for by lexer
struct Keyword {
	inline Keyword(token type, std::string keyword) : type(type), value(std::move(keyword)) {};
	token type;
	std::string value;
};

template <typename K, typename V>
std::unordered_map<V, K> invert_map(const std::unordered_map<K, V>& map) {
    std::unordered_map<V, K> inverted_map;
    for (const auto& pair : map) {
        inverted_map[pair.second] = pair.first;
    }
    return inverted_map;
}

inline int MAX_CALLBACK_LINES = 4990;
inline int MAX_UI_CONTROLS = 999;
inline int MAX_ARRAY_ELEMENTS = 1000000;

inline std::unordered_set<char> BINARY_OPERATORS = {'-', '+', '/', '*', '&'};
inline std::unordered_set<char> PARENTH = {'(',')', '[', ']'};
inline std::unordered_set<char> VAR_IDENT = {'$', '~', '@'};
inline std::unordered_set<char> ARRAY_IDENT = {'%', '?', '!'}; //int, real, string
inline std::unordered_map<std::string, token> TYPES = {{"$", token::INT}, {"~", token::FLOAT}, {"@", token::STRING}, {"%", token::INT}, {"?", token::FLOAT}, {"!", token::STRING}};
inline std::unordered_set<char> COMMENT_START = {'{', '/'};
inline std::unordered_set<char> COMPARISON_OPERATORS_START = {'<', '>', '=', '#'};
inline std::unordered_set<std::string> UI_CONTROLS = {"ui_label", "ui_button", "ui_switch", "ui_slider", "ui_menu",
										   "ui_value_edit", "ui_waveform", "ui_wavetable",
										   "ui_knob", "ui_table", "ui_xy",
										   "ui_text_edit", "ui_level_meter", "ui_file_selector",
										   "ui_panel", "ui_mouse_area"};
inline std::unordered_map<std::string, token> DECLARATION_SYNTAX = {{"declare", token::DECLARE}, {"define", token::DEFINE}, {"const", token::CONST}, {"polyphonic", token::POLYPHONIC},
                                                  {"read", token::READ},{"pers", token::PERS}, {"instpers", token::INSTPERS}, {"local", token::LOCAL}, {"global", token::GLOBAL}};
inline std::unordered_map<std::string, token> PREPROCESSOR_SYNTAX = {{"#pragma", token::PRAGMA}, {"import", token::IMPORT}, {"as", token::AS}, {"on", token::ON},
												   {"iterate_macro", token::ITERATE_MACRO}, {"literate_macro", token::LITERATE_MACRO},
													{"START_INC", token::START_INC}, {"END_INC", token::END_INC}, {"SET_CONDITION", token::SET_CONDITION}, {"RESET_CONDITION", token::RESET_CONDITION},
                                                   {"USE_CODE_IF", token::USE_CODE_IF}, {"USE_CODE_IF_NOT", token::USE_CODE_IF_NOT}, {"END_USE_CODE", token::END_USE_CODE}};
inline std::unordered_map<std::string, token> STATEMENT_SYNTAX = {{"to", token::TO}, {"downto", token::DOWNTO}, {"step", token::STEP}, {"else", token::ELSE},
																  {"case", token::CASE}, {"in", token::IN}, {"default", token::DEFAULT}};
inline std::unordered_map<std::string, token> FUNCTION_SYNTAX = {{"override", token::OVERRIDE}, {"call", token::CALL}, {"return", token::RETURN}};
inline std::unordered_map<std::string, token> OBJECT_SYNTAX = {{"new", token::NEW}, /*{"delete", token::DELETE},*/ {"nil", token::NIL}};
inline std::unordered_map<std::string, token> BOOLEAN_SYNTAX = {{"false", token::FALSE}, {"true", token::TRUE}};


// control statements that also have an end
inline std::unordered_map<std::string, token> END_STATEMENTS = {{"end function", token::END_FUNCTION}, {"end for", token::END_FOR}, {"end while", token::END_WHILE},
											  {"end if", token::END_IF}, {"end select", token::END_SELECT}, {"end const", token::END_CONST},
                                              {"end list", token::END_LIST}, {"end family", token::END_FAMILY}, {"end struct", token::END_STRUCT},
											  {"end macro", token::END_MACRO}, {"end taskfunc", token::END_TASKFUNC}, {"end component", token::END_COMPONENT}};
inline std::unordered_map<std::string, token> STATEMENTS = {{"function", token::FUNCTION}, {"for", token::FOR}, {"while", token::WHILE}, {"if", token::IF},
                                          {"select", token::SELECT}, {"const", token::CONST}, {"list", token::LIST}, {"family", token::FAMILY},
                                          {"struct", token::STRUCT}, {"macro", token::MACRO}, {"taskfunc", token::TASKFUNC}, {"component", token::COMPONENT}};
inline std::unordered_set<std::string> CALLBACKS = {"init", "note", "release", "midi_in", "controller",
											 "rpn", "nrpn", "ui_update", "_pgs_changed", "pgs_changed",
											 "poly_at", "listener", "async_complete", "persistence_changed", "ui_control", "ui_controls"};
inline std::unordered_set<std::string> RESTRICTED_CALLBACKS = {"init", "persistence_changed", "ui_control", "pgs_changed", "async_complete"};

inline std::unordered_set<std::string> BUILTIN_CONDITIONS = {"NO_SYS_SCRIPT_GROUP_START", "NO_SYS_SCRIPT_PEDAL", "NO_SYS_SCRIPT_RLS_TRIG"};

inline std::unordered_map<token, std::vector<std::string>> PERSISTENCE_TOKENS = {{token::READ, {"make_persistent", "read_persistent_var"}},
															{token::PERS, {"make_persistent"}},
															{token::INSTPERS, {"make_instr_persistent"}}};

/// string->Token operator maps
inline std::unordered_map<std::string, token> BITWISE_OPERATORS = {{".and.", token::BIT_AND}, {".or.", token::BIT_OR}, {".not.", token::BIT_NOT}, {".xor.", token::BIT_XOR}};
inline std::unordered_map<std::string, token> BOOL_OPERATORS = {{"and", token::BOOL_AND}, {"or", token::BOOL_OR}, {"not", token::BOOL_NOT}, {"xor", token::BOOL_XOR}};
inline std::unordered_map<std::string, token> MATH_OPERATORS = {{"-", token::SUB}, {"+", token::ADD}, {"/", token::DIV}, {"*", token::MULT}, {"mod", token::MODULO}};
inline std::unordered_map<std::string, token> UNARY_OPERATORS = {{"-", token::SUB}, {".not.", token::BIT_NOT}, {"not", token::BOOL_NOT}};
inline std::unordered_map<std::string, token> COMPARISON_OPERATORS = {{"<", token::LESS_THAN}, {">", token::GREATER_THAN}, {"=", token::EQUAL}, {"<=", token::LESS_EQUAL}, {">=", token::GREATER_EQUAL}, {"#", token::NOT_EQUAL}};
inline std::unordered_map<std::string, token> STRING_OPERATOR = {{"&", token::STRING_OP}};
inline const std::unordered_map<std::string, token> ALL_OPERATORS = []{
    std::unordered_map<std::string, token> ops = BITWISE_OPERATORS;
    ops.insert(MATH_OPERATORS.begin(), MATH_OPERATORS.end());
    ops.insert(BOOL_OPERATORS.begin(), BOOL_OPERATORS.end());
    return ops;
}();

// Hilfsfunktion zum Extrahieren der Tokens aus Maps
inline std::unordered_set<token> extract_tokens_from_map(const std::unordered_map<std::string, token>& map) {
	std::unordered_set<token> tokens;
	for (const auto& pair : map) {
		tokens.insert(pair.second);
	}
	return tokens;
}

/// Token sets for token operator types
inline const std::unordered_set<token> BITWISE_TOKENS = extract_tokens_from_map(BITWISE_OPERATORS);
inline const std::unordered_set<token> BOOL_TOKENS = extract_tokens_from_map(BOOL_OPERATORS);
inline const std::unordered_set<token> MATH_TOKENS = extract_tokens_from_map(MATH_OPERATORS);
inline const std::unordered_set<token> UNARY_TOKENS = extract_tokens_from_map(UNARY_OPERATORS);
inline const std::unordered_set<token> COMPARISON_TOKENS = extract_tokens_from_map(COMPARISON_OPERATORS);
inline const std::unordered_set<token> STRING_TOKENS = extract_tokens_from_map(STRING_OPERATOR);

/// Token->string maps reversed for generating vanilla KSP Code
inline static std::unordered_map<token, std::string> GENERATE_BITWISE_OPERATORS = {{token::BIT_AND, ".and."}, {token::BIT_OR, ".or."}, {token::BIT_NOT, ".not."}, {token::BIT_XOR, ".xor."}};
inline static std::unordered_map<token, std::string> GENERATE_BOOL_OPERATORS = {{token::BOOL_AND, "and"}, {token::BOOL_OR, "or"}, {token::BOOL_NOT, "not"}, {token::BOOL_XOR, "xor"}};
inline static std::unordered_map<token, std::string> GENERATE_MATH_OPERATORS = {{token::SUB, "-"}, {token::ADD, "+"}, {token::DIV, "/"}, {token::MULT, "*"}, {token::MODULO, "mod"}};
inline static std::unordered_map<token, std::string> GENERATE_UNARY_OPERATORS = {{token::SUB, "-"}, {token::BIT_NOT, ".not."}, {token::BOOL_NOT, "not"}};
inline static std::unordered_map<token, std::string> GENERATE_COMPARISON_OPERATORS = {{token::LESS_THAN, "<"}, {token::GREATER_THAN, ">"}, {token::EQUAL, "="}, {token::LESS_EQUAL, "<="}, {token::GREATER_EQUAL, ">="}, {token::NOT_EQUAL, "#"}};
inline static std::unordered_map<token, std::string> GENERATE_STRING_OPERATOR = {{token::STRING_OP, "&"}};
inline static std::unordered_map<token, std::string> GENERATE_ALL_OPERATORS = []{
	std::unordered_map<token, std::string> ops;
	ops.insert(GENERATE_BITWISE_OPERATORS.begin(), GENERATE_BITWISE_OPERATORS.end());
	ops.insert(GENERATE_BOOL_OPERATORS.begin(), GENERATE_BOOL_OPERATORS.end());
	ops.insert(GENERATE_MATH_OPERATORS.begin(), GENERATE_MATH_OPERATORS.end());
	ops.insert(GENERATE_UNARY_OPERATORS.begin(), GENERATE_UNARY_OPERATORS.end());
	ops.insert(GENERATE_COMPARISON_OPERATORS.begin(), GENERATE_COMPARISON_OPERATORS.end());
	ops.insert(GENERATE_STRING_OPERATOR.begin(), GENERATE_STRING_OPERATOR.end());
	return ops;
}();
