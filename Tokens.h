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


#endif //COMPILER_TOKENS_H
