//
// Created by Mathias Vatter on 03.06.23.
//

#include "Lexer.h"
#include <algorithm>
#include <vector>

Lexer::Lexer(const char* &input) : input(input), pos(0), line(1) {
    current_char = *input;
    input_length = strlen(input);

    tokenize();

	for (auto & token: tokens) {
        if (token.type != COMMENT && token.type != LINEBRK)
		    std::cout << token << '\n';
	}
	std::cout << std::endl;
}

void Lexer::tokenize() {
    while (pos < input_length) {
        if (current_char == '/' || current_char == '{') {
            get_comment();
        } else if (current_char == '\n') {
            get_linebreak();
        } else if (is_keyword_or_num()) {
            get_keyword_or_num();
        } else if (is_string()) {
            get_string();
        } else if (contains(current_char, MATH) && peek() != '>') {
            get_math();
        } else if (contains(current_char, PARENTH)) {
            get_parenth();
        } else if (current_char == ':' && peek() == '=') {
            get_assignment();
        } else if (current_char == '-' && peek() == '>') {
            get_arrow();
        } else if (contains(current_char, COMPARISON_START)) {
            get_comparison();
        } else if (current_char == '.' && peek() != '.') {
            get_bitwise_operator();
        } else if (current_char == '.' && peek() == '.' && peek(2) == '.') {
            get_line_continuation();
        } else if (current_char == ',') {
            get_comma();
        } else
            get_invalid();
    }
}

void Lexer::next_char(int chars) {
    for(int i = 0; i<chars; i++) {
        if (pos >= input_length) break;
        buffer += current_char;
        pos++;
        input++;
        this->current_char = *input;
    }
}

void Lexer::skip_whitespace() {
    while (is_space(current_char)) {
        if (pos >= input_length) break;
        buffer += current_char;
        pos++;
        input++;
        this->current_char = *input;
    }
}

char Lexer::peek(int ahead) const {
    if (input_length < pos+ahead)
        return 0;
    return input[ahead];
}

void Lexer::get_invalid() {
	flush_buffer();
    next_char();
	tokens.emplace_back(INVALID, buffer, line);
	skip_whitespace();
}

void Lexer::get_comment() {
	flush_buffer();
	if (current_char == '{') { // multi-line ksp style
		while (current_char != '}') {
			next_char();
            if (current_char == '\n') line++;
		}
		next_char();
	} else if (current_char == '/') {
        // if one-line comment c++ style
        if (peek() == '/') {
            while (current_char != '\n') {
                next_char();
            }
            // skip nex_char(); so that the \n can be tokenized
            // if multi-line comment c++ style
        } else if (peek() == '*') {
            while (current_char != '*' and peek() != '/') {
                next_char();
                if (current_char == '\n') line++;
            }
            next_char();
        }
    }
    if (not buffer.empty())
	    tokens.emplace_back(COMMENT, this->buffer, this->line);
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

void Lexer::get_math() {
    flush_buffer();
    token tok;
    if (current_char == '-') {
        tok = SUB;
    } else if (current_char == '+') {
        tok = ADD;
    } else if (current_char == '/') {
        tok = DIV;
    } else if (current_char == '*') {
        tok = MULT;
    }
    tokens.emplace_back(tok, std::string(1,this->current_char), this->line);
    next_char();
    skip_whitespace();
}

void Lexer::get_parenth() {
    flush_buffer();
    token tok;
    if (current_char == '(')
        tok = OPEN_PARENTH;
    else if (current_char == ')')
        tok = CLOSED_PARENTH;
    else if (current_char == '[')
        tok = OPEN_BRACKET;
    else if (current_char == ']')
        tok = CLOSED_BRACKET;
    tokens.emplace_back(tok, std::string(1,this->current_char), this->line);
    next_char();
    skip_whitespace();
}

void Lexer::get_assignment() {
    tokens.emplace_back(ASSIGN, ":=", this->line);
    next_char(2);
    skip_whitespace();
}

void Lexer::get_arrow() {
    tokens.emplace_back(ARROW, "->", this->line);
    next_char(2);
    skip_whitespace();
}

bool Lexer::contains(char c, std::vector<char>& vec) {
    return std::any_of(vec.begin(), vec.end(), [&](const auto& ch) {return ch == c;});
}

bool Lexer::is_keyword_or_num() {
    return std::isalnum(current_char) || current_char == '_' || contains(current_char, VAR_IDENT);
}

void Lexer::get_keyword_or_num() {
    flush_buffer();
    while(std::isdigit(current_char)) {
        next_char();
    }
    // check if is float or bitwise operator
    if (current_char == '.') {
        next_char();
        // is float
        if (std::isdigit(current_char)) {
            while (std::isdigit(current_char)) {
                next_char();
            }
            tokens.emplace_back(FLOAT, this->buffer, this->line);
        } else
            std::cerr << buffer << " is not a known keyword" << std::endl;
    // check if next char is _ or text
    } else if (is_keyword_or_num()) {
        while (is_keyword_or_num()) {
            next_char();
        }
        if (is_hexadecimal(buffer)) {
            tokens.emplace_back(HEXADECIMAL, this->buffer, this->line);
        } else if (is_binary(buffer)) {
            tokens.emplace_back(BINARY, this->buffer, this->line);
        } else if (is_callback_start()) {
            tokens.pop_back();
            tokens.emplace_back(BEGIN_CALLBACK, "on " + this->buffer, this->line);
        } else if (is_callback_end()) {
            tokens.pop_back();
            tokens.emplace_back(END_CALLBACK, "end " + this->buffer, this->line);
        } else if (buffer == "mod") {
            tokens.emplace_back(MODULO, this->buffer, this->line);
        } else if (contains(BOOL_OPERATORS, buffer)) {
            token tok = get_token_type(BOOL_OPERATORS, buffer);
            tokens.emplace_back(tok, this->buffer, this->line);
        } else if (contains(STATEMENTS, buffer)) {
            // get end statements
            if (tokens.back().val == "end") {
                tokens.pop_back();
                auto val = "end " + buffer;
                token tok = get_token_type(END_STATEMENTS, val);
                tokens.emplace_back(tok, val, line);
            } else {
                token tok = get_token_type(STATEMENTS, buffer);
                tokens.emplace_back(tok, buffer, line);
            }
        } else
            tokens.emplace_back(KEYWORD, this->buffer, this->line);
    } else // is probably int
        tokens.emplace_back(INT, this->buffer, this->line);
    skip_whitespace();
}


void Lexer::get_linebreak() {
    tokens.emplace_back(LINEBRK, "\n", this->line);
    this->line++;
    next_char();
    skip_whitespace();
}

void Lexer::get_comparison() {
    flush_buffer();
    token tok;
    if (current_char == '>' ) {
        if (peek() == '=') {
            tok = GREATER_EQUAL;
            next_char();
        } else
            tok = GREATER_THAN;
    } else if (current_char == '<') {
        if (peek() == '=') {
            tok = LESS_EQUAL;
            next_char();
        } else
            tok = LESS_THAN;
    } else if (current_char == '=') {
        tok = EQUAL;
    } else std::cerr << "Error in comparison operator" << std::endl;
    tokens.emplace_back(tok, std::string(1,this->current_char), this->line);
    next_char();
    skip_whitespace();
}

void Lexer::get_comma() {
    flush_buffer();
    tokens.emplace_back(COMMA, std::string(1,this->current_char), this->line);
    next_char();
    skip_whitespace();
}

void Lexer::get_line_continuation() {
    flush_buffer();
    next_char(3);
    tokens.emplace_back(LINE_CONTINUE, buffer, line);
    skip_whitespace();

}

void Lexer::get_bitwise_operator() {
    flush_buffer();
    next_char();
    while(std::isalpha(current_char)) {
        next_char();
    }
    next_char();
    if (contains(BITWISE_OPERATORS, buffer)) {
        token tok = get_token_type(BITWISE_OPERATORS, buffer);
        tokens.emplace_back(tok, buffer, this->line);
    } else {
        std::cerr << buffer << " is not a known keyword" << std::endl;
    }
    next_char();
    skip_whitespace();
}

bool Lexer::is_space(const char &ch) {
	return ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ';
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

bool Lexer::is_binary(const std::string& str) {
    // Überprüfen, ob der String mit "b" beginnt oder auf "b" endet
    if (str.back() == 'b' || str[0] == 'b') {
        return true;
    }
        // Überprüfen, ob der String mit einer Ziffer beginnt, "b" in der Mitte hat und mit "h" endet
    else if (str.size() > 2 && isdigit(str[0]) && str.find('b') != std::string::npos && str.back() == 'h') {
        return true;
    }
    return false;
}

bool Lexer::is_hexadecimal(const std::string& str) {
    // Überprüfen, ob der String mit "0x" beginnt oder mit einem Ziffer gefolgt von "h"
    return (str.substr(0, 2) == "0x" || (str.size() > 1 && str.back() == 'h' && isdigit(str[0]) && str.find('b') == std::string::npos));
}

bool Lexer::is_callback_start() {
    return (tokens.back().val == "on" && contains(CALLBACKS, buffer));
}

bool Lexer::is_callback_end() {
    return tokens.back().val == "end" && buffer == "on";
}

bool Lexer::contains(const std::vector<std::string> &vec, const std::string &value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

bool Lexer::contains(const std::vector<keyword> &vec, const std::string &value) {
    return std::find_if(vec.begin(), vec.end(), [&value](const keyword& kw) {
        return kw.value == value;
    }) != vec.end();
}

token Lexer::get_token_type(const std::vector<keyword> &vec, const std::string &value) {
    auto it = std::find_if(vec.begin(), vec.end(), [&value](const keyword& kw) {
        return kw.value == value;
    });

    if (it != vec.end()) {
        return it->type;
    }
    return INVALID; // Rückgabe von UNKNOWN, wenn die Zeichenkette nicht gefunden wird
}


/*
 * TOKEN STRUCT
 */
Token::Token(token type, const std::string &val, size_t line): line(line), type(type), val(val) {}

std::ostream &operator<<(std::ostream &os, const Token &tok) {
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
