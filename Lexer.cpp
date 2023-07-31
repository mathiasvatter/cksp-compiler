//
// Created by Mathias Vatter on 03.06.23.
//

#include "Lexer.h"

Lexer::Lexer(const std::string &input) : input(input), pos(0) {

    this->prev_char = input[0];
    this->current_char = input[1];

    while (current_char <= input.length()) {
        tokens.push_back(next_token());
    }
}

Lexer::Token Lexer::next_token() {
    get_comment();

}

Lexer::Token Lexer::get_comment() {
    flush_buffer();
    // one-line comment
    if (prev_char == '/' && current_char == '/') {
        while (prev_char != '\n') {
            next_char();
        }
    }
    // multi-line c++ style
    if (prev_char == '/' && current_char == '*') {
        while (prev_char != '*' && current_char != '/') {
            next_char();
        }
    }
    // multi-line ksp style
    if (prev_char == '{') {
        while (prev_char != '}') {
            next_char();
        }
    }
    return Lexer::Token(COMMENT, this->buffer, this->line);
}


void Lexer::next_char(int chars) {
    for(int i = 0; i<chars-1; i++) {
        this->prev_char = input[pos];
        pos++;
        this->current_char = input[pos];
        buffer += prev_char;
        // skip whitespace
        while (std::isspace(static_cast<unsigned char>(prev_char))) {
            this->prev_char = input[pos];
            pos++;
            this->current_char = input[pos];
            buffer += prev_char;
        }
        if (this->prev_char == '\n') this->line++;
    }
}

Lexer::Token Lexer::get_callback() {
    flush_buffer();
    if (look_ahead(3).find("on ") == 0) {
        next_char(3);
        while (!std::isspace(prev_char)) {
            next_char();
        }
    }
    return Lexer::Token(CALLBACK, this->buffer, this->line);
}

void Lexer::flush_buffer() {
    this->buffer.clear();
}

std::string Lexer::look_ahead(int chars) {
    std::string la_buffer;
    if (input.length() < chars)
        return input;
    for (int i = 0; i < chars-1; i++) {
        la_buffer += input[this->pos + i];
    }
    return la_buffer;
}


Lexer::Token::Token(token type, const std::string &val, size_t line): line(line), type(type), val(val) {}

std::ostream &operator<<(std::ostream &os, const Lexer::Token &tok) {
    os << "Type: " << tok.type << ", Value: " << tok.val << ", Line: " << tok.line;
    return os;
}
