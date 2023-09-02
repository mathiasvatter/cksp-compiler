//
// Created by Mathias Vatter on 03.06.23.
//

#include "Tokenizer.h"
#include <algorithm>
#include <utility>
#include <vector>

/*
 * TOKEN STRUCT
 */
Token::Token(token type, std::string  val, size_t line): line(line), type(type), val(std::move(val)) {}

std::ostream &operator<<(std::ostream &os, const Token &tok) {
    os << "Type: " << tok.type << " | Value: " << tok.val << " | Line: " << tok.line;
    return os;
}

/*
 * Tokenizer Functions
 */
Tokenizer::Tokenizer(std::string input) : pos(0), line(1){
    this->input = std::move(input);
    this->input += '\n';
	current_char = this->input.at(pos);
	input_length = this->input.size();

}

std::vector<Token> Tokenizer::tokenize() {
    while (pos < input_length-1) {
        if (current_char == '/' && (peek() =='*' || peek() =='/') || current_char == '{') {
            get_comment();
        } else if (current_char == '\n') {
            get_linebreak();
        } else if (is_keyword_or_num()) {
            get_keyword_or_num();
        } else if (is_string()) {
            get_string();
        } else if (contains(MATH, current_char) && peek() != '>') {
            get_math();
        } else if (contains(PARENTH, current_char)) {
            get_parenth();
        } else if (current_char == ':' && peek() == '=') {
            get_assignment();
        } else if (current_char == '-' && peek() == '>') {
            get_arrow();
        } else if (contains(COMPARISON_START, current_char)) {
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
    tokens.emplace_back(END_TOKEN, buffer, line);
    return tokens;
}

void Tokenizer::consume(int chars) {
    for(int i = 0; i<chars; i++) {
        if (pos+1 >= input_length) {
			auto err_msg = "Reached end of file.";
			CompileError(ErrorType::TokenError, err_msg, line, "end token", std::string(1,current_char)).print();
			exit(EXIT_FAILURE);
		}
        buffer += current_char;
        pos++;
        current_char = input.at(pos);
    }
}

void Tokenizer::skip_whitespace() {
    while (is_space(current_char)) {
        if (pos+1 >= input_length) {
			auto err_msg = "Reached end of file.";
			CompileError(ErrorType::TokenError, err_msg, line, "end token", std::string(1,current_char)).print();
			exit(EXIT_FAILURE);
		}
        buffer += current_char;
        pos++;
        current_char = input.at(pos);
    }
}

char Tokenizer::peek(int ahead) const {
    if (input_length <= pos+ahead) {
		auto err_msg = "Reached end of file.";
		CompileError(ErrorType::TokenError, err_msg, line, "end token", std::string(1,current_char)).print();
		exit(EXIT_FAILURE);
	}

    return input.at(pos+ahead);
}

void Tokenizer::get_invalid() {
	flush_buffer();
	consume();
	tokens.emplace_back(INVALID, buffer, line);
	auto err_msg = "Found invalid token.";
	CompileError(ErrorType::TokenError, err_msg, line, "valid token", buffer).print();
	exit(EXIT_FAILURE);
	skip_whitespace();
}

void Tokenizer::get_comment() {
	flush_buffer();
	if (current_char == '{') { // multi-line ksp style
		while (current_char != '}') {
			consume();
            if (current_char == '\n') line++;
		}
		consume();
	} else if (current_char == '/') {
        // if one-line comment c++ style
        if (peek() == '/') {
            while (current_char != '\n') {
				consume();
            }
            // skip nex_char(); so that the \n can be tokenized
            // if multi-line comment c++ style
        } else if (peek() == '*') {
            while (current_char != '*' or peek() != '/') {
				consume();
                if (current_char == '\n') line++;
            }
			consume(2);
        }
    }
    if (not buffer.empty())
//	    tokens.emplace_back(COMMENT, this->buffer, this->line);
	skip_whitespace();
}

bool Tokenizer::is_string() const {
	return current_char == '\'' || current_char == '"';
}

void Tokenizer::get_string() {
	flush_buffer();
	char starting_char = current_char;
	consume();
	while(current_char != starting_char) {
		consume();
	}
	consume();
	tokens.emplace_back(STRING, this->buffer, this->line);
	skip_whitespace();
}

void Tokenizer::get_math() {
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
	consume();
    skip_whitespace();
}

void Tokenizer::get_parenth() {
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
	consume();
    skip_whitespace();
}

void Tokenizer::get_assignment() {
    tokens.emplace_back(ASSIGN, ":=", this->line);
	consume(2);
    skip_whitespace();
}

void Tokenizer::get_arrow() {
    tokens.emplace_back(ARROW, "->", this->line);
	consume(2);
    skip_whitespace();
}

bool Tokenizer::is_keyword_or_num() const {
    return std::isalnum(current_char) || current_char == '_' || contains(VAR_IDENT, current_char) || contains(
            ARRAY_IDENT, current_char);
}

void Tokenizer::get_keyword_or_num() {
    flush_buffer();
    while(std::isdigit(current_char)) {
		consume();
    }
    // check if is float or bitwise operator
    if (current_char == '.') {
		consume();
        // is float
        if (std::isdigit(current_char)) {
            while (std::isdigit(current_char)) {
				consume();
            }
            tokens.emplace_back(FLOAT, this->buffer, this->line);
        } else {
			auto err_msg = "Found unknown keyword.";
			CompileError(ErrorType::TokenError, err_msg, line, "valid keyword", buffer).print();
			exit(EXIT_FAILURE);
		}
    // check if next char is _ or text
    } else if (is_keyword_or_num()) {
        consume();
        while (std::isalnum(current_char) || current_char == '_') {
            consume();
        }
        // catch var identifier in the middle of keyword
        if (contains(VAR_IDENT, current_char) || contains(ARRAY_IDENT, current_char)) {
            consume();
            auto err_msg = "Incorrect placement of variable/array identifier";
            CompileError(ErrorType::TokenError, err_msg, line, "valid variable", buffer).print();
            exit(EXIT_FAILURE);
        }
        token tok;
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
            tok = get_token_type(BOOL_OPERATORS, buffer);
            tokens.emplace_back(tok, this->buffer, this->line);
        } else if (contains(STATEMENTS, buffer)) {
            // get begin statements
            tok = get_token_type(STATEMENTS, buffer);
            std::string val = buffer;
            // get end statements
            if (!tokens.empty()) {
                if (tokens.back().val == "end") {
                    tokens.pop_back();
                    val = "end " + buffer;
                    tok = get_token_type(END_STATEMENTS, val);
                }
            }
            tokens.emplace_back(tok, val, line);
        } else if (contains(STATEMENT_SYNTAX, buffer)) {
            tok = get_token_type(STATEMENT_SYNTAX, buffer);
            tokens.emplace_back(tok, buffer, line);
        } else if (contains(UI_CONTROLS, buffer)) {
			tok = get_token_type(UI_CONTROLS, buffer);
			tokens.emplace_back(tok, buffer, line);
		} else if (contains(IMPORT_SYNTAX, buffer)) {
			tok = get_token_type(IMPORT_SYNTAX, buffer);
			tokens.emplace_back(tok, buffer, line);
		} else if (contains(DECLARATION_SYNTAX, buffer)) {
			tok = get_token_type(DECLARATION_SYNTAX, buffer);
			tokens.emplace_back(tok, buffer, line);
        } else {
            tokens.emplace_back(KEYWORD, buffer, line);
        }
        // see if char after keyword is dot
        if (current_char == '.') {
            consume();
            tokens.emplace_back(DOT, ".", line);
            if (std::isalnum(current_char) || current_char == '_') {
                flush_buffer();
                while(std::isalnum(current_char) || current_char == '_') {
                    consume();
                }
                tokens.emplace_back(KEYWORD, buffer, line);
            } else {
                auto err_msg = "Found unknown keyword.";
                CompileError(ErrorType::TokenError, err_msg, line, "valid keyword", buffer).print();
                exit(EXIT_FAILURE);
            }
        }
    } else // is probably int
        tokens.emplace_back(INT, buffer, line);
    skip_whitespace();
}


void Tokenizer::get_linebreak() {
    tokens.emplace_back(LINEBRK, "linebreak", this->line);
    this->line++;
	consume();
    skip_whitespace();
}

void Tokenizer::get_comparison() {
    flush_buffer();
    token tok;
    if (current_char == '>' ) {
        if (peek() == '=') {
            tok = GREATER_EQUAL;
			consume();
        } else
            tok = GREATER_THAN;
    } else if (current_char == '<') {
        if (peek() == '=') {
            tok = LESS_EQUAL;
			consume();
        } else
            tok = LESS_THAN;
    } else if (current_char == '=') {
        tok = EQUAL;
    } else {
		auto err_msg = "Unknown comparison operator.";
		CompileError(ErrorType::TokenError, err_msg, line, "<, >, =", buffer).print();
		exit(EXIT_FAILURE);
	}
    tokens.emplace_back(tok, std::string(1,this->current_char), this->line);
	consume();
    skip_whitespace();
}

void Tokenizer::get_comma() {
    flush_buffer();
    tokens.emplace_back(COMMA, std::string(1,this->current_char), this->line);
	consume();
    skip_whitespace();
}

void Tokenizer::get_line_continuation() {
    flush_buffer();
	consume(3);
    tokens.emplace_back(LINE_CONTINUE, buffer, line);
    skip_whitespace();

}

void Tokenizer::get_bitwise_operator() {
    flush_buffer();
	consume();
    while(std::isalpha(current_char)) {
		consume();
    }
	consume();
    if (contains(BITWISE_OPERATORS, buffer)) {
        token tok = get_token_type(BITWISE_OPERATORS, buffer);
        tokens.emplace_back(tok, buffer, this->line);
    } else {
		auto err_msg = "Found unknown keyword. Keywords starting with dots are not allowed.";
		CompileError(ErrorType::TokenError, err_msg, line, "valid keyword", buffer).print();
		exit(EXIT_FAILURE);
    }
    skip_whitespace();
}

bool Tokenizer::is_space(const char &ch) {
	return ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ';
}

void Tokenizer::flush_buffer() {
    this->buffer.clear();
}

bool Tokenizer::is_binary(const std::string& str) {
	// Überprüfen, ob der String mit "b" beginnt und nur Ziffern enthält
	if (str[0] == 'b') {
		return std::all_of(str.begin() + 1, str.end(), [](char c) { return c == '0' || c == '1'; });
	}
		// Überprüfen, ob der String auf "b" endet und nur Ziffern enthält
	else if (str.back() == 'b') {
		return std::all_of(str.begin(), str.end() - 1, [](char c) { return c == '0' || c == '1'; });
	}
		// Überprüfen, ob der String mit einer Ziffer beginnt, "b" in der Mitte hat und mit "h" endet
	else if (str.size() > 2 && isdigit(str[0]) && str.find('b') != std::string::npos && str.back() == 'h') {
		size_t b_pos = str.find('b');
		return std::all_of(str.begin() + 1, str.begin() + b_pos, [](char c) { return c == '0' || c == '1'; });
	}
	return false;
}

bool Tokenizer::is_hexadecimal(const std::string& str) {
    // Überprüfen, ob der String mit "0x" beginnt oder mit einem Ziffer gefolgt von "h"
    return (str.substr(0, 2) == "0x" || (str.size() > 1 && str.back() == 'h' && isdigit(str[0]) && str.find('b') == std::string::npos));
}

bool Tokenizer::is_callback_start() {
    if (!tokens.empty())
        return (contains(CALLBACKS, buffer) && tokens.back().val == "on");
    return false;
}

bool Tokenizer::is_callback_end() {
    if (!tokens.empty())
        return tokens.back().val == "end" && buffer == "on";
    return false;
}

token Tokenizer::get_token_type(const std::vector<Keyword> &vec, const std::string &value) {
    auto it = std::find_if(vec.begin(), vec.end(), [&value](const Keyword& kw) {
        return kw.value == value;
    });

    if (it != vec.end()) {
        return it->type;
    }
    return INVALID; // Rückgabe von UNKNOWN, wenn die Zeichenkette nicht gefunden wird
}





