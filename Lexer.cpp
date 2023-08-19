//
// Created by Mathias Vatter on 03.06.23.
//

#include "Lexer.h"
#include <algorithm>
#include <vector>

Lexer::Lexer(const char* &input) : input(input), pos(0), line(0) {

//    this->prev_char = input[0];
    this->current_char = *input;
    this->input_length = strlen(input);

    while (pos < input_length) {
		next_token();
    }

	for (auto & token: tokens) {
		std::cout << token << '\n';
	}
	std::cout << std::endl;
}

void Lexer::next_token() {
	if (is_comment()) get_comment();
	else if (is_string()) get_string();
//	else if (is_num()) get_num();
    else if (is_math()) get_math();
    else if (is_bracket()) get_bracket();
    else if (is_assignment()) get_assignment();
    else if (is_arrow()) get_arrow();
//	else if (is_callback()) get_callback();
    else if (is_variable()) get_text_or_num();
	else {get_invalid();}
}

void Lexer::get_invalid() {
	flush_buffer();
//	while (!is_space(current_char)) {
		next_char();
//	}
	tokens.emplace_back(INVALID, buffer, line);
	skip_whitespace();
}

bool Lexer::is_comment() {
	auto la = look_ahead(2);
	return (la.find("//") == 0 || la.find('{') == 0 || la.find("/*") == 0);
}

void Lexer::get_comment() {
	flush_buffer();
	auto la = look_ahead(2);
	// one-line comment
	if (la.find("//") == 0) {
		while (current_char != '\n') {
			next_char();
		}
	} else if (la.find("/*") == 0) { // multi-line c++ style
		while (look_ahead(2).find("*/") != 0) {
			next_char();
		}
	} else if (current_char == '{') { // multi-line ksp style
		while (current_char != '}') {
			next_char();
		}
		next_char();
	}
	tokens.emplace_back(COMMENT, this->buffer, this->line);
	skip_whitespace();
}

bool Lexer::is_callback() {
	return (look_ahead(3).find("on ") == 0) || (look_ahead(6).find("end on") == 0);
}

void Lexer::get_callback() {
    flush_buffer();
	token tok = END_CALLBACK;
	if (current_char == 'o') tok = CALLBACK;
	while (current_char != '\n') {
		next_char();
	}
    tokens.emplace_back(tok, this->buffer, this->line);
	skip_whitespace();
}

bool Lexer::is_string() const {
	return current_char == '\'' || current_char == '"';
}

void Lexer::get_string() {
	flush_buffer();
	char starting_char = current_char;
	next_char();
	while(current_char != starting_char) {
		next_char();
	}
	next_char();
	tokens.emplace_back(STRING, this->buffer, this->line);
	skip_whitespace();
}

void Lexer::get_literal() {

    for (int i = KEYWORD; tokenStrings[i] != nullptr; i++) {
        std::cout << strlen(tokenStrings[i]) << std::endl;
    }
}

bool Lexer::is_math() const {
    return current_char == '-' || current_char == '+' || current_char == '/' || current_char == '*';
}

void Lexer::get_math() {
    for (auto &ch: MATH) {
        if (current_char == ch) {
            flush_buffer();
            for (int i = 0; tokenStrings[i] != nullptr; i++) {
                if (*tokenStrings[i] == ch)
                    tokens.emplace_back((token)i, std::string(1,this->current_char), this->line);
                else
                    stderr;
            }
            next_char();
            skip_whitespace();
        }
    }
}

bool Lexer::is_bracket() const {
    return current_char == '(' || current_char == ')' || current_char == '[' || current_char == ']' ||
    current_char == '{' || current_char == '}';
}

void Lexer::get_bracket() {
    token tok;
    if (current_char == '(')
        tok = OPEN_PARENTH;
    else if (current_char == ')')
        tok = CLOSED_PARENTH;
    else if (current_char == '[')
        tok = OPEN_BRACKET;
    else if (current_char == ']')
        tok = CLOSED_BRACKET;
    else if (current_char == '{')
        tok = OPEN_CURLY;
    else if (current_char == '}')
        tok = CLOSED_CURLY;
    tokens.emplace_back(tok, std::string(1,this->current_char), this->line);
    next_char();
    skip_whitespace();
}

bool Lexer::is_assignment() {
    return look_ahead(2) == ":=";
}

void Lexer::get_assignment() {
    tokens.emplace_back(ASSIGN, ":=", this->line);
    next_char(2);
    skip_whitespace();
}

bool Lexer::is_arrow() {
    return look_ahead(2) == "->";
}

void Lexer::get_arrow() {
    tokens.emplace_back(ARROW, "->", this->line);
    next_char(2);
    skip_whitespace();
}

bool Lexer::is_var_identifier(char c) {
    return std::any_of(VAR_IDENT.begin(), VAR_IDENT.end(), [&](const auto& ch) {return ch == c;});
}

bool Lexer::is_variable() {
    return std::isalnum(current_char) || current_char == '_' || is_var_identifier(current_char);
}

void Lexer::get_text_or_num() {
    flush_buffer();
    while(std::isdigit(current_char)) {
        next_char();
    }
    // check if is float
    if (current_char == '.') {
        next_char();
        while(std::isdigit(current_char)) {
            next_char();
        }
        tokens.emplace_back(FLOAT, this->buffer, this->line);
    // check if next char is _ or text
    } else if (is_variable()) {
        while (is_variable()) {
            next_char();
        }
        // detect callback!
        if (buffer[0] == 'o' && buffer[1] == 'n' && std::iswblank(current_char)) {
            next_char();
            while (!is_space(current_char)) {
                next_char();
            }
            tokens.emplace_back(CALLBACK, this->buffer, this->line);
        } else if (buffer[0] == 'e' && buffer[1] == 'n' && buffer[2] == 'd' && std::iswblank(current_char)) {
            next_char();
            while (!is_space(current_char)) {
                next_char();
            }
            tokens.emplace_back(END_CALLBACK, this->buffer, this->line);
        } else
            tokens.emplace_back(KEYWORD, this->buffer, this->line);
    } else // is probably int
        tokens.emplace_back(INT, this->buffer, this->line);
    skip_whitespace();

}

void Lexer::next_char(int chars) {
    for(int i = 0; i<chars; i++) {
		if (pos >= input_length) break;
        buffer += current_char;
        pos++;
        input++;
        this->current_char = *input;
        if (this->current_char == '\n') {this->line++; }
    }
}

void Lexer::skip_whitespace() {
	while (is_space(current_char)) {
		if (pos >= input_length) break;
		buffer += current_char;
		pos++;
        input++;
		this->current_char = *input;
		if (this->current_char == '\n') {this->line++;}
	}
}

bool Lexer::is_space(const char &ch) {
	return ch == '\n' || ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ';
}

void Lexer::flush_buffer() {
    this->buffer.clear();
}

std::string Lexer::look_ahead(int chars) {
    std::string la_buffer;
    int real_chars = chars;
    if (input_length < pos+chars)
        real_chars = input_length - pos;
    for (int i = 0; i < real_chars; i++) {
        la_buffer += input[i];
    }
    return la_buffer;
}

/*
 * TOKEN STRUCT
 */
Lexer::Token::Token(token type, const std::string &val, size_t line): line(line), type(type), val(val) {}

std::ostream &operator<<(std::ostream &os, const Lexer::Token &tok) {
    os << "Type: " << tok.type << ", Value: " << tok.val << ", Line: " << tok.line;
    return os;
}


#define STRING(name, str) str,
const char *tokenStrings[] = {
	ENUM_LIST(STRING)
};
#undef STRING

std::ostream &operator<<(std::ostream &os, const token &tok) {
	os << tokenStrings[tok];
	return os;
}
