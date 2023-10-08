//
// Created by Mathias Vatter on 03.06.23.
//

#include "Tokenizer.h"
#include <algorithm>
#include <utility>
#include <vector>
#include <fstream>
#include <sstream>

/*
 * TOKEN STRUCT
 */
Token::Token(token type, std::string  val, size_t line, std::string &file)
            : line(line), type(type), val(std::move(val)), file(file) {}

std::ostream &operator<<(std::ostream &os, const Token &tok) {
    os << "Type: " << tok.type << " | Value: " << tok.val << " | Line: " << tok.line;
    return os;
}

/*
 * Tokenizer Functions
 */
Tokenizer::Tokenizer(std::string file) : pos(0), line(1){
    this->file = std::move(file);
    this->input = read_file(this->file);
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
            fix_line_continuation();
        } else if (is_keyword_or_num()) {
            get_keyword_or_num();
        } else if (is_string()) {
            get_string();
        } else if (contains(BINARY_OPERATORS, current_char) && peek() != '>') {
            get_binary_operators();
        } else if (contains(PARENTH, current_char)) {
            get_parenth();
        } else if (current_char == ':' && peek() == '=') {
            get_assignment();
        } else if (current_char == '-' && peek() == '>') {
            get_arrow();
        } else if (contains(COMPARISON_OPERATORS_START, current_char)) {
            get_comparison_operators();
        } else if (current_char == '.' && peek() != '.') {
            get_bitwise_operator();
        } else if (current_char == '.' && peek() == '.' && peek(2) == '.') {
            get_line_continuation();
        } else if (current_char == ',') {
            get_comma();
        } else
            get_invalid();
    }
    tokens.emplace_back(token::LINEBRK, "\n", line, file);
    tokens.emplace_back(END_TOKEN, "", 0, file);
    return tokens;
}

void Tokenizer::consume(int chars) {
    for(int i = 0; i<chars; i++) {
        if (pos+1 >= input_length) {
			auto err_msg = "Reached end of file.";
			CompileError(ErrorType::TokenError, err_msg, line, "end token", std::string(1,current_char), file).print();
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
			CompileError(ErrorType::TokenError, err_msg, line, "end token", std::string(1,current_char), file).print();
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
		CompileError(ErrorType::TokenError, err_msg, line, "end token", std::string(1,current_char), file).print();
		exit(EXIT_FAILURE);
	}

    return input.at(pos+ahead);
}

void Tokenizer::get_invalid() {
	flush_buffer();
	consume();
	tokens.emplace_back(INVALID, buffer, line, file);
	auto err_msg = "Found invalid token.";
	CompileError(ErrorType::TokenError, err_msg, line, "valid token", buffer, file).print();
	exit(EXIT_FAILURE);
	skip_whitespace();
}

void Tokenizer::get_comment() {
	flush_buffer();
    int nesting_level = 0; // nesting levels for {{}} comments
    if (current_char == '{') { // multi-line ksp style
        nesting_level++;
        while (nesting_level > 0) {
            consume();
            if (current_char == '\n') line++;
            if (current_char == '{') {
                nesting_level++;
            } else if (current_char == '}') {
                nesting_level--;
            }
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
	tokens.emplace_back(STRING, buffer, line, file);
	skip_whitespace();
}

void Tokenizer::get_binary_operators() {
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
    } else if (current_char == '&') {
        tok = STRING_OPERATOR;
    }
    tokens.emplace_back(tok, std::string(1,current_char), line, file);
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
    tokens.emplace_back(tok, std::string(1,current_char), line, file);
	consume();
    skip_whitespace();
}

void Tokenizer::get_assignment() {
    tokens.emplace_back(ASSIGN, ":=", line, file);
	consume(2);
    skip_whitespace();
}

void Tokenizer::get_arrow() {
    tokens.emplace_back(ARROW, "->", line, file);
	consume(2);
    skip_whitespace();
}

bool Tokenizer::is_keyword_or_num() const {
    bool is_keyword_or_num = std::isalnum(current_char) || current_char == '_' || contains(VAR_IDENT, current_char) || contains(
            ARRAY_IDENT, current_char);
    bool is_macro = current_char =='#' and (std::isalnum(peek()) || peek() == '_' || contains(VAR_IDENT, peek()) || contains(
            ARRAY_IDENT, peek()));
    return is_keyword_or_num or is_macro;
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
            tokens.emplace_back(FLOAT, buffer, line, file);
        } else {
			auto err_msg = "Found unknown keyword.";
			CompileError(ErrorType::TokenError, err_msg, line, "valid keyword", buffer, file).print();
			exit(EXIT_FAILURE);
		}
    // check if next char is _ or text
    } else if (is_keyword_or_num()) {
        if(current_char =='#') consume(); //consume # for macro iteration
        consume(); //consume possible identifier
        while (std::isalnum(current_char) || current_char == '_') {
            consume();
        }
        // catch var identifier in the middle of keyword
        if (contains(VAR_IDENT, current_char) || contains(ARRAY_IDENT, current_char)) {
            consume();
            auto err_msg = "Incorrect placement of variable/array identifier";
            CompileError(ErrorType::TokenError, err_msg, line, "valid variable", buffer, file).print();
            exit(EXIT_FAILURE);
        }
        token tok;
        if (is_hexadecimal(buffer)) {
            tokens.emplace_back(HEXADECIMAL, buffer, line, file);
        } else if (is_binary(buffer)) {
            tokens.emplace_back(BINARY, buffer, line, file);
        } else if (is_callback_start()) {
            tokens.pop_back();
            tokens.emplace_back(BEGIN_CALLBACK, "on " + buffer, line, file);
        } else if (is_callback_end()) {
            tokens.pop_back();
            tokens.emplace_back(END_CALLBACK, "end " + buffer, line, file);
        } else if (buffer == "mod") {
            tokens.emplace_back(MODULO, buffer, line, file);
        } else if (contains(BOOL_OPERATORS, buffer)) {
            tok = get_token_type(BOOL_OPERATORS, buffer);
            tokens.emplace_back(tok, buffer, line, file);
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
            tokens.emplace_back(tok, val, line, file);
        } else if (contains(STATEMENT_SYNTAX, buffer)) {
            tok = get_token_type(STATEMENT_SYNTAX, buffer);
            tokens.emplace_back(tok, buffer, line, file);
        } else if (contains(UI_CONTROLS, buffer)) {
			tok = token::UI_CONTROL;
			tokens.emplace_back(tok, buffer, line, file);
		} else if (contains(IMPORT_SYNTAX, buffer)) {
			tok = get_token_type(IMPORT_SYNTAX, buffer);
			tokens.emplace_back(tok, buffer, line, file);
		} else if (contains(DECLARATION_SYNTAX, buffer)) {
			tok = get_token_type(DECLARATION_SYNTAX, buffer);
			tokens.emplace_back(tok, buffer, line, file);
        } else if (contains(FUNCTION_SYNTAX, buffer)) {
            tok = get_token_type(FUNCTION_SYNTAX, buffer);
            tokens.emplace_back(tok, buffer, line, file);
        } else {
            while (current_char == '.') {
                consume();
    //            tokens.emplace_back(DOT, ".", line);
                if (std::isalnum(current_char) || current_char == '_') {
//                    flush_buffer();
                    while(std::isalnum(current_char) || current_char == '_') {
                        consume();
                    }
//                    tokens.emplace_back(KEYWORD, buffer, line);
                } else {
                    auto err_msg = "Found unknown keyword.";
                    CompileError(ErrorType::TokenError, err_msg, line, "valid keyword", buffer, file).print();
                    exit(EXIT_FAILURE);
                }
            }
            if(current_char == '#') consume();
            tokens.emplace_back(KEYWORD, buffer, line, file);
        }
        // see if char after keyword is dot
    } else // is probably int
        tokens.emplace_back(INT, buffer, line, file);
    skip_whitespace();
}


void Tokenizer::get_linebreak() {
    tokens.emplace_back(LINEBRK, "linebreak", line, file);
    this->line++;
	consume();
    skip_whitespace();
}

void Tokenizer::get_comparison_operators() {
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
    } else if (current_char == '#') {
        // NOT
    } else {
		auto err_msg = "Unknown comparison operator.";
		CompileError(ErrorType::TokenError, err_msg, line, "<, >, =", buffer, file).print();
		exit(EXIT_FAILURE);
	}
    tokens.emplace_back(token::COMPARISON, std::string(1,current_char), line, file);
	consume();
    skip_whitespace();
}

void Tokenizer::get_comma() {
    flush_buffer();
    tokens.emplace_back(COMMA, std::string(1,current_char), line, file);
	consume();
    skip_whitespace();
}

void Tokenizer::get_line_continuation() {
    flush_buffer();
	consume(3);
    tokens.emplace_back(LINE_CONTINUE, buffer, line, file);
    skip_whitespace();

}

void Tokenizer::fix_line_continuation() {
    // to be handled right after token LINEBRK is inserted
    if(tokens.size() >= 2) {
        if (tokens.back().type == token::LINEBRK && tokens.at(tokens.size()-2).type == token::LINE_CONTINUE) {
            tokens.pop_back();
            tokens.pop_back();
        }
    }
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
        tokens.emplace_back(tok, buffer, line, file);
    } else {
		auto err_msg = "Found unknown keyword. Keywords starting with dots are not allowed.";
		CompileError(ErrorType::TokenError, err_msg, line, "valid keyword", buffer, file).print();
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
    // Überprüfen, ob der String mit "b" beginnt und nur 0 und 1 enthält
    bool starts_with_b = str.size() > 1 && str[0] == 'b' &&
                       std::all_of(str.begin() + 1, str.end(), [](char c){ return c == '0' || c == '1'; });

    // Überprüfen, ob der String mit "b" endet und nur 0 und 1 enthält
    bool ends_width_b = str.size() > 1 && str.back() == 'b' &&
                     std::all_of(str.begin(), str.end() - 1, [](char c){ return c == '0' || c == '1'; });
    // XOR-Prüfung
    return starts_with_b xor ends_width_b;
}

bool Tokenizer::is_hexadecimal(const std::string& str) {
    // Überprüfen, ob der String mit "0x" beginnt xor mit einer ziffer beginnt und mit h endet
    return (str.substr(0, 2) == "0x") xor (str.size() > 1 && str.back() == 'h' && isdigit(str[0]));
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
    return INVALID;
}

std::string Tokenizer::read_file(const std::string& filename) {
    std::ifstream f(filename);
    std::stringstream buf;
    if (f.is_open()) {
        buf << f.rdbuf();
        f.close();
    } else {
        std::cout << "Unable to open file\n";
    }
    return buf.str();
}





