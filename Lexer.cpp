//
// Created by Mathias Vatter on 03.06.23.
//

#include "Lexer.h"

Lexer::Lexer(const std::string &input) : input(input), pos(0), line(0) {

//    this->prev_char = input[0];
    this->current_char = input[0];

    while (pos < input.length()) {
		next_token();
    }

	for (auto & token: tokens) {
		std::cout << token << '\n';
	}
	std::cout << std::endl;
}

void Lexer::next_token() {
	if (is_comment()) {get_comment();}
	else if (is_callback()) {get_callback();}
	else if (is_string()) {get_string();}
	else if (is_num()) {get_num();}
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

bool Lexer::is_declaration() {
	return (look_ahead(4).find("decl") == 0);
}

void Lexer::get_declaration() {
	flush_buffer();
	while(!is_space(current_char)) {
		next_char();
	}
	tokens.emplace_back(KEYWORDS, this->buffer, this->line);
	skip_whitespace();
}

bool Lexer::is_num() const {
	return std::isdigit(current_char);
}

void Lexer::get_num() {
	flush_buffer();
	while(std::isdigit(current_char)) {
		next_char();
	}
	if (current_char == '.') {
		next_char();
		while(std::isdigit(current_char)) {
			next_char();
		}
		tokens.emplace_back(FLOAT, this->buffer, this->line);
	} else
		tokens.emplace_back(INT, this->buffer, this->line);
	skip_whitespace();
}

void Lexer::next_char(int chars) {
    for(int i = 0; i<chars; i++) {
		if (pos >= input.size()) break;
        buffer += current_char;
        pos++;
        this->current_char = input[pos];
        if (this->current_char == '\n') {this->line++; }
    }
}

void Lexer::skip_whitespace() {
	while (is_space(current_char)) {
		if (pos >= input.size()) break;
		buffer += current_char;
		pos++;
		this->current_char = input[pos];
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
    if (input.length() < pos+chars)
        return input.substr(pos);
    for (int i = 0; i < chars; i++) {
        la_buffer += input[this->pos + i];
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
