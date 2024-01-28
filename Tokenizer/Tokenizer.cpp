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
Token::Token(token type, std::string  val, size_t line, size_t pos, const std::string &file)
            : line(line), pos(pos), type(type), val(std::move(val)), file(file) {}

std::ostream &operator<<(std::ostream &os, const Token &tok) {
    os << "Type: " << tok.type << " | Value: " << tok.val << " | Line: " << tok.line;
    return os;
}

/*
 * Tokenizer Functions
 */
Tokenizer::Tokenizer(const std::string& input, const std::string& file, FileType file_type)
    : m_pos(0), m_line(1), m_line_pos(1) {
    m_current_file = file;
    m_is_json = false;
    if(file_type == FileType::nckp) m_is_json = true;

    m_input = input;
    m_input += '\n';
    m_current_char = m_input.at(m_pos);
    m_input_length = m_input.size();
}

std::vector<Token> Tokenizer::tokenize() {
    if(m_input.empty())
        CompileError(ErrorType::TokenError, "Missing input file to tokenize.", 0, "<input file>", "", m_current_file).exit();

    while (m_pos < m_input_length - 1) {
        if(is_pragma()) {
            consume();
            consume();
        }
        else if (peek() == '/' && (peek(1) == '*' || peek(1) == '/') || (peek() == '{' && !m_is_json)) {
            get_comment();
        } else if (peek() == '\n') {
            get_linebreak();
            fix_line_continuation();
        } else if (contains(BINARY_OPERATORS, peek()) && peek(1) != '>') {
            get_binary_operators();
        } else if (contains(PARENTH, peek())) {
            get_parenth();
        } else if (peek() == ':' && peek(1) == '=') {
            get_assignment();
        } else if (peek() == '-' && peek(1) == '>') {
            get_arrow();
        } else if (is_keyword_or_num()) {
            get_keyword_or_num();
        } else if (is_string()) {
            get_string();
        } else if (contains(COMPARISON_OPERATORS_START, peek())) {
            get_comparison_operators();
        } else if (peek() == '.' && peek(1) != '.') {
            get_bitwise_operator();
        } else if (peek() == '.' && peek(1) == '.' && peek(2) == '.') {
            get_line_continuation();
        } else if (peek() == ',') {
            get_comma();
        } else if(is_space(peek())) {
            skip_whitespace();
        } else if(peek() == ':') {
            get_type();
        } else if(m_is_json and (peek() == '}' || peek() == '{')) {
            get_curly_brackets();
        } else
            get_invalid();
    }
    m_tokens.emplace_back(token::LINEBRK, "\n", m_line, m_line_pos-std::string("\n").length(), m_current_file);
    m_tokens.emplace_back(END_TOKEN, "", 0, m_line_pos, m_current_file);
    return m_tokens;
}

char Tokenizer::consume() {
    if (m_pos < m_input_length) {
        m_buffer += peek();
        m_line_pos++;
        m_current_char = m_input.at(m_pos + 1);
        return m_input.at(m_pos++);
    }
    auto err_msg = "Reached end of file.";
    CompileError(ErrorType::TokenError, err_msg, m_line, "end token", std::string(1, peek()), m_current_file).print();
    exit(EXIT_FAILURE);
}

void Tokenizer::skip_whitespace() {
    while (is_space(peek())) {
        if (m_pos + 1 >= m_input_length) {
			auto err_msg = "Reached end of file.";
			CompileError(ErrorType::TokenError, err_msg, m_line, "end token", std::string(1, peek()), m_current_file).print();
			exit(EXIT_FAILURE);
		}
//        m_buffer += peek();
//        m_pos++;
//        m_current_char = m_input.at(m_pos);
        consume();
    }
}

char Tokenizer::peek(int ahead) const {
    if (m_input_length <= m_pos + ahead) {
		auto err_msg = "Reached end of file.";
		CompileError(ErrorType::TokenError, err_msg, m_line, "end token", std::string(1, peek()), m_current_file).print();
		exit(EXIT_FAILURE);
	}
    return m_input.at(m_pos + ahead);
}

void Tokenizer::get_invalid() {
	flush_buffer();
	consume();
	m_tokens.emplace_back(INVALID, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
	auto err_msg = "Found invalid token.";
	CompileError(ErrorType::TokenError, err_msg, m_line, "valid token", m_buffer, m_current_file).exit();
	skip_whitespace();
}

bool Tokenizer::is_pragma() {
    return peek(0) == '/' and peek(1) == '/' and peek(2) == '#' and
            peek(3) == 'p' and peek(4) == 'r' and peek(5) == 'a' and
            peek(6) == 'g' and peek(7) == 'm' and peek(8) == 'a';
}


void Tokenizer::get_comment() {
	flush_buffer();
    int nesting_level = 0; // nesting levels for {{}} comments
    if (peek() == '{') { // multi-m_line ksp style
        nesting_level++;
        while (nesting_level > 0) {
            consume();
            if (peek() == '\n') {m_line++; m_line_pos = 1;}
            if (peek() == '{') {
                nesting_level++;
            } else if (peek() == '}') {
                nesting_level--;
            }
        }
        consume();
	} else if (peek() == '/') {
        // if one-m_line comment c++ style
        if (peek(1) == '/') {
            while (peek() != '\n') {
				consume();
            }
            // skip nex_char(); so that the \n can be tokenized
            // if multi-m_line comment c++ style
        } else if (peek(1) == '*') {
            while (peek() != '*' or peek(1) != '/') {
				consume();
                if (peek() == '\n') {m_line++; m_line_pos = 1;}
            }
			consume();
            consume();
        }
    }
//    if (not m_buffer.empty())
//	    m_tokens.emplace_back(COMMENT, m_buffer, this->m_line_pos-m_buffer.length(), m_line);
	skip_whitespace();
}

bool Tokenizer::is_string() const {
	return peek() == '\'' || peek() == '"';
}

void Tokenizer::get_string() {
	flush_buffer();
	char starting_char = peek();
	consume();
	while(peek() != starting_char) {
        if (peek() == '\\' and peek(1) == starting_char) {
            consume();
        }
		consume();
	}
	consume();
	m_tokens.emplace_back(STRING, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
	skip_whitespace();
}

void Tokenizer::get_binary_operators() {
    flush_buffer();
    token tok;
    if (peek() == '-') {
        tok = SUB;
    } else if (peek() == '+') {
        tok = ADD;
    } else if (peek() == '/') {
        tok = DIV;
    } else if (peek() == '*') {
        tok = MULT;
    } else if (peek() == '&') {
        tok = STRING_OP;
    }
	consume();
    m_tokens.emplace_back(tok, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}

void Tokenizer::get_parenth() {
    flush_buffer();
    token tok;
    if (peek() == '(')
        tok = OPEN_PARENTH;
    else if (peek() == ')')
        tok = CLOSED_PARENTH;
    else if (peek() == '[')
        tok = OPEN_BRACKET;
    else if (peek() == ']')
        tok = CLOSED_BRACKET;
	consume();
    m_tokens.emplace_back(tok, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}

void Tokenizer::get_curly_brackets() {
    flush_buffer();
    token tok;
    if (peek() == '{')
        tok = OPEN_CURLY;
    else if (peek() == '}')
        tok = CLOSED_CURLY;
    consume();
    m_tokens.emplace_back(tok, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}


void Tokenizer::get_assignment() {
	consume();
    consume();
    m_tokens.emplace_back(ASSIGN, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}

void Tokenizer::get_arrow() {
	consume();
    consume();
    m_tokens.emplace_back(ARROW, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}

bool Tokenizer::is_keyword_or_num() const {
    bool is_keyword_or_num = std::isalnum(peek()) || peek() == '_' || contains(VAR_IDENT, peek()) || contains(
            ARRAY_IDENT, peek());
    bool is_macro = peek() == '#' and (std::isalnum(peek(1)) || peek(1) == '_' || contains(VAR_IDENT, peek(1)) || contains(
            ARRAY_IDENT, peek(1)));
    return is_keyword_or_num or is_macro;
}

void Tokenizer::get_keyword_or_num() {
    flush_buffer();
    while(std::isdigit(peek())) {
		consume();
    }
    // check if is float or bitwise operator
    if (peek() == '.') {
		consume();
        // is float
        if (std::isdigit(peek())) {
            while (std::isdigit(peek())) {
				consume();
            }
            m_tokens.emplace_back(FLOAT, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else {
			auto err_msg = "Found unknown keyword.";
			CompileError(ErrorType::TokenError, err_msg, m_line, "valid keyword", m_buffer, m_current_file).print();
			exit(EXIT_FAILURE);
		}
    // check if next char is _ or text
    } else if (is_keyword_or_num()) {
//        if(peek() =='#') consume(); //consume # for macro iteration
        consume(); //consume possible identifier
        while (std::isalnum(peek()) || peek() == '_' || peek() == '#') {
            consume();
        }
        // catch var identifier in the middle of keyword
        if (contains(VAR_IDENT, peek()) || contains(ARRAY_IDENT, peek())) {
            consume();
            auto err_msg = "Incorrect placement of variable/array identifier";
            CompileError(ErrorType::TokenError, err_msg, m_line, "valid variable", m_buffer, m_current_file).print();
            exit(EXIT_FAILURE);
        }
        token tok;
        if (is_hexadecimal(m_buffer)) {
            m_tokens.emplace_back(HEXADECIMAL, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (is_binary(m_buffer)) {
            m_tokens.emplace_back(BINARY, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (is_callback_start()) {
            m_tokens.pop_back();
            m_tokens.emplace_back(BEGIN_CALLBACK, "on " + m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (is_callback_end()) {
            m_tokens.pop_back();
            m_tokens.emplace_back(END_CALLBACK, "end " + m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (m_buffer == "mod") {
            m_tokens.emplace_back(MODULO, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (auto type = get_token_type(BOOL_OPERATORS, m_buffer)) {
//            tok = get_token_type(BOOL_OPERATORS, m_buffer);
            m_tokens.emplace_back(*type, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (auto type = get_token_type(STATEMENTS, m_buffer)) {
            // get begin statements
//            tok = get_token_type(STATEMENTS, m_buffer);
            std::string val = m_buffer;
            // get end statements
            if (!m_tokens.empty()) {
                if (m_tokens.back().val == "end") {
                    m_tokens.pop_back();
                    val = "end " + m_buffer;
                    type = get_token_type(END_STATEMENTS, val);
                }
            }
            m_tokens.emplace_back(*type, val, m_line, m_line_pos-val.length(), m_current_file);
        } else if (auto type = get_token_type(STATEMENT_SYNTAX, m_buffer)) {
//            tok = get_token_type(STATEMENT_SYNTAX, m_buffer);
            m_tokens.emplace_back(*type, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (contains(UI_CONTROLS, m_buffer)) {
			tok = token::UI_CONTROL;
			m_tokens.emplace_back(tok, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
		} else if (auto type = get_token_type(PREPROCESSOR_SYNTAX, m_buffer)) {
//			tok = get_token_type(PREPROCESSOR_SYNTAX, m_buffer);
			m_tokens.emplace_back(*type, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
		} else if (auto type = get_token_type(DECLARATION_SYNTAX, m_buffer)) {
//			tok = get_token_type(DECLARATION_SYNTAX, m_buffer);
			m_tokens.emplace_back(*type, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else if (auto type = get_token_type(FUNCTION_SYNTAX, m_buffer)) {
//            tok = get_token_type(FUNCTION_SYNTAX, m_buffer);
            m_tokens.emplace_back(*type, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        } else {
            while (peek() == '.') {
                consume();
                if (std::isalnum(peek()) || peek() == '_' || peek() == '#') {
                    while(std::isalnum(peek()) || peek() == '_' || peek() == '#') {
                        consume();
                    }
                } else {
                    auto err_msg = "Found unknown keyword.";
                    CompileError(ErrorType::TokenError, err_msg, m_line, "valid keyword", m_buffer, m_current_file).print();
                    exit(EXIT_FAILURE);
                }
            }
            m_tokens.emplace_back(KEYWORD, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
        }
        // see if char after keyword is dot
    } else // is probably int
        m_tokens.emplace_back(INT, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}


void Tokenizer::get_linebreak() {
    flush_buffer();
	consume();
    m_tokens.emplace_back(LINEBRK, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    m_line++; m_line_pos = 1;
    skip_whitespace();
}

void Tokenizer::get_comparison_operators() {
    flush_buffer();
    token tok;
//    std::string token;
    if (peek() == '>' ) {
        if (peek(1) == '=') {
            tok = GREATER_EQUAL;
			consume();
        } else
            tok = GREATER_THAN;
    } else if (peek() == '<') {
        if (peek(1) == '=') {
            tok = LESS_EQUAL;
			consume();
        } else
            tok = LESS_THAN;
    } else if (peek() == '=') {
        tok = EQUAL;
    } else if (peek() == '#') {
        // NOT
    } else {
		auto err_msg = "Unknown comparison operator.";
		CompileError(ErrorType::TokenError, err_msg, m_line, "<, >, =", m_buffer, m_current_file).print();
		exit(EXIT_FAILURE);
	}
	consume();
    m_tokens.emplace_back(token::COMPARISON, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}

void Tokenizer::get_comma() {
    flush_buffer();
	consume();
    m_tokens.emplace_back(COMMA, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}

void Tokenizer::get_type() {
    flush_buffer();
    consume();
    m_tokens.emplace_back(TYPE, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();
}


void Tokenizer::get_line_continuation() {
    flush_buffer();
	consume();
    consume();
    consume();
    m_tokens.emplace_back(LINE_CONTINUE, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    skip_whitespace();

}

void Tokenizer::fix_line_continuation() {
    // to be handled right after token LINEBRK is inserted
    if(m_tokens.size() >= 2) {
        if (m_tokens.back().type == token::LINEBRK && m_tokens.at(m_tokens.size() - 2).type == token::LINE_CONTINUE) {
            m_tokens.pop_back();
            m_tokens.pop_back();
        }
    }
}

void Tokenizer::get_bitwise_operator() {
    flush_buffer();
	consume();
    while(std::isalpha(peek())) {
		consume();
    }
	consume();
    if (auto tok = get_token_type(BITWISE_OPERATORS, m_buffer)) {
//        tok = get_token_type(BITWISE_OPERATORS, m_buffer);
        m_tokens.emplace_back(*tok, m_buffer, m_line, m_line_pos-m_buffer.length(), m_current_file);
    } else {
		auto err_msg = "Found unknown keyword. Keywords starting with dots are not allowed.";
		CompileError(ErrorType::TokenError, err_msg, m_line, "valid keyword", m_buffer, m_current_file).exit();
    }
    skip_whitespace();
}

bool Tokenizer::is_space(const char &ch) {
	return ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ';
}

void Tokenizer::flush_buffer() {
    this->m_buffer.clear();
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
    if (!m_tokens.empty())
        return (contains(CALLBACKS, m_buffer) && m_tokens.back().val == "on");
    return false;
}

bool Tokenizer::is_callback_end() {
    if (!m_tokens.empty())
        return m_tokens.back().val == "end" && m_buffer == "on";
    return false;
}

std::string read_file(const std::string& filename) {
    std::ifstream f(filename);
    std::stringstream buf;
    if (!f.is_open()) {
        CompileError(ErrorType::FileError, "Unable to open file.", -1, "valid path/valid *.ksp file", filename, "").exit();
    } else {
        buf << f.rdbuf();
        f.close();
    }
    return buf.str();
}





