//
// Created by Mathias Vatter on 03.06.23.
//

#include "Tokenizer.h"
#include <algorithm>
#include <utility>
#include <vector>

#include "../utils/StringUtils.h"

/*
 * TOKEN STRUCT
 */
Token::Token(token type, std::string  val, size_t line, size_t pos, const std::string &file)
            : type(type), val(std::move(val)), line(line), pos(pos), file(file) {}

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

    m_input = input;
    m_input += '\n';
    m_current_char = m_input.at(m_pos);
    m_input_length = m_input.size();
}

std::vector<Token> Tokenizer::tokenize() {
	is_in_fstring = false;
    if(m_input.empty())
        CompileError(ErrorType::TokenError, "Missing input file to tokenize.", 0, "<input file>", "", m_current_file).exit();

	token_loop();

    add_token(token::LINEBRK, "\n");
    // m_tokens.emplace_back(token::LINEBRK, "\n", m_line, m_line_pos-std::string("\n").length(), m_current_file);
    // m_tokens.emplace_back(token::END_TOKEN, "", 0, m_line_pos, m_current_file);
    add_token(token::END_TOKEN, m_buffer);
    return m_tokens;
}

void Tokenizer::token_loop() {
	while (m_pos < m_input_length - 1) {
		if(is_pragma()) {
			consume();
			consume();
		}
		else if (peek() == '/' && (peek(1) == '*' || peek(1) == '/') || peek() == '{') {
			get_comment();
		} else if (peek() == '\n') {
			get_linebreak();
			fix_line_continuation();
		} else if (PARENTH.contains(peek())) {
			get_parenth();
		} else if (peek() == 'f' && (peek(1) == '\'' || peek(1) == '"')) {
			flush_buffer();
			consume(); // consume f
			fstring_starting_char.push(consume());
			add_token(token::FSTRING_START, m_buffer);
			get_format_string();
		} else if (peek() == '>' && !fstring_starting_char.empty()) {
			flush_buffer();
			consume(); // consume >
			add_token(token::FSTRING_EXPR_STOP, m_buffer);
			flush_buffer();
			get_format_string();
		} else if (peek() == ':' && peek(1) == '=') {
			get_assignment();
		} else if (peek() == '-' && peek(1) == '>') {
			get_arrow();
		} else if (BINARY_OPERATORS.contains(peek()) && peek(1) != '>') {
			get_binary_operators();
		} else if (is_keyword_or_num()) {
			get_keyword_or_num();
		} else if (is_string()) {
			get_string();
		} else if (COMPARISON_OPERATORS_START.contains(peek())) {
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
		} else if (peek() == '?') {
			get_ternary_operator();
		} else {
			get_invalid();
		}
	}
}

char Tokenizer::consume() {
    if (m_pos + 1 < m_input_length) {
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

void Tokenizer::add_token(token type, std::string val) {
    size_t pos = m_line_pos - val.length();
    m_tokens.emplace_back(type, std::move(val), m_line, pos, m_current_file);
    flush_buffer();
}

char Tokenizer::peek(const int ahead) const {
    if (m_input_length <= m_pos + ahead) {
		auto error = CompileError(
		    ErrorType::TokenError,
		    "Reached end of file.",
		    m_line,
		    "end token",
		    std::string(1,m_current_char),
		    m_current_file);
		error.exit();
	}
    return m_input.at(m_pos + ahead);
}

void Tokenizer::get_invalid() {
	flush_buffer();
	consume();
	add_token(token::INVALID, m_buffer);
	auto got = !m_buffer.empty() ? m_buffer : std::string(1, m_current_char);
	auto error = CompileError(ErrorType::TokenError, "", m_line, "valid token", got, m_current_file);
	error.exit();
	error.add_message("Found invalid token: " + got);
	skip_whitespace();
}

bool Tokenizer::is_pragma() const {
    auto workaround_pragma = peek(0) == '/' and peek(1) == '/' and peek(2) == '#' and
            peek(3) == 'p' and peek(4) == 'r' and peek(5) == 'a' and
            peek(6) == 'g' and peek(7) == 'm' and peek(8) == 'a';
	if(workaround_pragma) {
		auto error = CompileError(ErrorType::CompileWarning, "", m_line, "", "//#pragma", m_current_file);
		error.m_message = "Found usage of //#pragma. Note that this is a workaround and will be removed in future versions.";
		error.print();
	}
	return workaround_pragma;
}


void Tokenizer::get_comment() {
	flush_buffer();
    int nesting_level = 0; // nesting levels for {{}} comments
    if (peek() == '{') { // multi-line ksp style
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
            // if multi-line comment c++ style
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
	add_token(token::STRING, m_buffer);
	skip_whitespace();
}

void Tokenizer::get_format_string() {
    while(peek() != fstring_starting_char.top()) {
        if (peek() == '\\' and peek(1) == fstring_starting_char.top()) {
            consume();
        }
    	if (peek() == '\\' and peek(1) == '<') {
    		consume();
    	}
    	if (peek() == '<') {
    		if (!m_buffer.empty()) {
    			add_token(token::STRING, m_buffer);
    		}
    		flush_buffer();
    		consume();
    		add_token(token::FSTRING_EXPR_START, m_buffer);
    		return;
    	}
    	consume();
    }
	add_token(token::STRING, m_buffer);
	consume(); // consume the fstring_starting_char
	add_token(token::FSTRING_STOP, m_buffer);
	flush_buffer();
	fstring_starting_char.pop();
	skip_whitespace();
}

void Tokenizer::get_ternary_operator() {
	flush_buffer();
	consume();
	add_token(token::TERNARY, m_buffer);
	skip_whitespace();
}

void Tokenizer::get_binary_operators() {
    flush_buffer();
    token tok;
    if (peek() == '-') {
        tok = token::SUB;
    } else if (peek() == '+') {
        tok = token::ADD;
    } else if (peek() == '/') {
        tok = token::DIV;
    } else if (peek() == '*') {
		if(peek(1) == '*') {
			consume();
			tok = token::EXP;
		} else {
        	tok = token::MULT;
		}
    } else if (peek() == '&') {
        tok = token::STRING_OP;
    }
	consume();
    add_token(tok, m_buffer);
    skip_whitespace();
}

void Tokenizer::get_compound_assignment_operators() {
	flush_buffer();
	token tok;

}

void Tokenizer::get_parenth() {
    flush_buffer();
	char p = consume();
    add_token(PARENTH.at(p), m_buffer);
    skip_whitespace();
}

void Tokenizer::get_assignment() {
	consume();
    consume();
    add_token(token::ASSIGN, m_buffer);
    skip_whitespace();
}

void Tokenizer::get_arrow() {
	consume();
    consume();
    add_token(token::ARROW, m_buffer);
    skip_whitespace();
}

bool Tokenizer::is_keyword_or_num() const {
    bool is_keyword_or_num = std::isalnum(peek()) || peek() == '_' || VAR_IDENT.contains(peek()) ||
            ARRAY_IDENT.contains(peek());
    bool is_macro = peek() == '#' and (std::isalnum(peek(1)) || peek(1) == '_' || VAR_IDENT.contains(peek(1)) ||
            ARRAY_IDENT.contains(peek(1)));
	bool is_float_start = peek() == '.' and std::isdigit(peek(1));
//	bool is_method_chain = peek() == '.' and (std::isalnum(peek(1)) || peek(1) == '_');
    return is_keyword_or_num or is_macro or is_float_start;
}

void Tokenizer::get_keyword_or_num() {
    flush_buffer();
	bool found_leading_digits = false;
	// Leading digits: parse number literal path
	while(std::isdigit(peek())) {
		consume();
		found_leading_digits = true;
	}

	// for 1e-3 style floats
	bool is_float = false;
	if (found_leading_digits and is_scientific_exponent_start()) {
		consume_exponent();
		is_float = true;
	}

    // check if is float or bitwise operator
	if (peek() == '.') {
		consume();
		if (std::isdigit(peek())) {
			while (std::isdigit(peek())) {
				consume();
			}
			// also allow an exponent after a leading-dot float like .5e2
			if (is_scientific_exponent_start()) {
				consume_exponent();
			}
			add_token(token::FLOAT, m_buffer);
		} else {
			auto err_msg = "Found unknown keyword.";
			CompileError(ErrorType::TokenError, err_msg, m_line, "valid keyword", m_buffer, m_current_file).exit();
		}
    // check if next char is _ or text
    } else if (is_keyword_or_num()) {
	    //        if(peek() =='#') consume(); //consume # for macro iteration
    	consume(); //consume possible identifier
    	while (std::isalnum(peek()) || peek() == '_' || peek() == '#') {
    		consume();
    	}
    	while (peek() == '.') {
    		consume();
    		if (std::isalnum(peek()) || peek() == '_' || peek() == '#') {
    			while(std::isalnum(peek()) || peek() == '_' || peek() == '#') {
    				consume();
    			}
    		} else {
    			auto err_msg = "Found unknown keyword.";
    			CompileError(ErrorType::TokenError, err_msg, m_line, "valid keyword", m_buffer, m_current_file).exit();
    		}
    	}
    	if (is_hexadecimal(m_buffer)) {
    		add_token(token::HEXADECIMAL, m_buffer);
    	} else if (is_binary(m_buffer)) {
    		add_token(token::BINARY, m_buffer);
    	} else if (is_callback_start()) {
    		m_tokens.pop_back();
    		add_token(token::BEGIN_CALLBACK, "on "+m_buffer);
    	} else if (is_callback_end()) {
    		m_tokens.pop_back();
    		add_token(token::END_CALLBACK, "end "+m_buffer);
    	} else if (m_buffer == "mod") {
    		add_token(token::MODULO, m_buffer);
    	} else if (auto type = get_token_type(BOOL_OPERATORS, m_buffer)) {
    		add_token(*type, m_buffer);
    	} else if (auto type = get_token_type(STATEMENTS, m_buffer)) {
    		// get end statements
    		if (!m_tokens.empty()) {
    			if (m_tokens.back().val == "end") {
    				m_tokens.pop_back();
    				m_buffer = "end " + m_buffer;
    				type = get_token_type(END_STATEMENTS, m_buffer);
    			}
    		}
    		add_token(*type, m_buffer);
    	} else if (auto type = get_token_type(STATEMENT_SYNTAX, m_buffer)) {
    		add_token(*type, m_buffer);
    	} else if (UI_CONTROLS.contains(m_buffer)) {
    		add_token(token::UI_CONTROL, m_buffer);
    	} else if (auto type = get_token_type(PREPROCESSOR_SYNTAX, m_buffer)) {
    		add_token(*type, m_buffer);
    	} else if (auto type = get_token_type(DECLARATION_SYNTAX, m_buffer)) {
    		add_token(*type, m_buffer);
    	} else if (auto type = get_token_type(FUNCTION_SYNTAX, m_buffer)) {
    		add_token(*type, m_buffer);
    	} else if (auto type = get_token_type(BOOLEAN_SYNTAX, m_buffer)) {
    		add_token(*type, m_buffer);
    	} else if (auto type = get_token_type(OBJECT_SYNTAX, m_buffer)) {
    		add_token(*type, m_buffer);
    	} else {

    		// try to fix #keyword into # keyword (two tokens)
    		const auto not_equal = GENERATE_COMPARISON_OPERATORS[token::NOT_EQUAL];
    		if (StringUtils::starts_with(m_buffer, not_equal)) {
    			if (StringUtils::count_char(m_buffer, not_equal[0]) % 2 == 1) {
    				add_token(token::NOT_EQUAL, not_equal);
    				m_buffer.erase(0, not_equal.length());
    			}
    		}

    		add_token(token::KEYWORD, m_buffer);
    	}
    	// see if char after keyword is dot
    } else if (is_float) {
		add_token(token::FLOAT, m_buffer);
    } else {
	    // is probably int
    	add_token(token::INT, m_buffer);
    }
    skip_whitespace();
}


void Tokenizer::get_linebreak() {
    flush_buffer();
	consume();
    add_token(token::LINEBRK, m_buffer);
    m_line++; m_line_pos = 1;
    skip_whitespace();
}

void Tokenizer::get_comparison_operators() {
    flush_buffer();
    token tok;
//    std::string token;
    if (peek() == '>' ) {
        if (peek(1) == '=') {
			tok = token::GREATER_EQUAL;
			consume();
		} else if(peek(1) == '>') {
			tok = token::SHIFT_RIGHT;
			consume();
			if (peek(1) == '>') {
				tok = token::SHIFT_RIGHT_LOGICAL;
				consume();
			}
        } else {
            tok = token::GREATER_THAN;
        }
    } else if (peek() == '<') {
        if (peek(1) == '=') {
			tok = token::LESS_EQUAL;
			consume();
		} else if(peek(1) == '<') {
			tok = token::SHIFT_LEFT;
			consume();
        } else
            tok = token::LESS_THAN;
    } else if (peek() == '=') {
        tok = token::EQUAL;
    } else if (peek() == '#') {
		tok = token::NOT_EQUAL;
        // NOT
    } else {
		auto err_msg = "Unknown comparison operator.";
		CompileError(ErrorType::TokenError, err_msg, m_line, "<, >, =", m_buffer, m_current_file).exit();
	}
	consume();
    add_token(tok, m_buffer);
    skip_whitespace();
}

void Tokenizer::get_comma() {
    flush_buffer();
	consume();
    add_token(token::COMMA, m_buffer);
    skip_whitespace();
}

void Tokenizer::get_type() {
    flush_buffer();
    consume();
    add_token(token::TYPE, m_buffer);
    skip_whitespace();
}


void Tokenizer::get_line_continuation() {
    flush_buffer();
	consume();
    consume();
    consume();
    add_token(token::LINE_CONTINUE, m_buffer);
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
	auto tok = get_token_type(BITWISE_OPERATORS, m_buffer+".");
//	if(peek() == '.') consume();
    if (tok and peek() == '.') {
		consume();
        add_token(*tok, m_buffer);
    } else {
		// method chaining
		while (std::isalnum(peek()) || peek() == '_' || peek() == '#') {
			consume();
		}
        // add_token(token::DOT, ".");
		m_tokens.emplace_back(token::DOT, ".", m_line, m_line_pos-m_buffer.length(), m_current_file);
        // add_token(token::KEYWORD, m_buffer.erase(0,1));
		m_tokens.emplace_back(token::KEYWORD, m_buffer.erase(0,1), m_line, m_line_pos-m_buffer.length()+1, m_current_file);
    }
    skip_whitespace();
}

bool Tokenizer::is_space(const char &ch) {
	return ch == '\t' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ';
}

void Tokenizer::flush_buffer() {
    m_buffer.clear();
}

bool Tokenizer::is_binary(const std::string& str) {
    // Überprüfen, ob der String mit "b" beginnt und nur 0 und 1 enthält
    bool starts_with_b = str.size() > 1 && str[0] == 'b' &&
                       std::all_of(str.begin() + 1, str.end(), [](char c){ return c == '0' || c == '1'; });

    // Überprüfen, ob der String mit "b" endet und nur 0 und 1 enthält
    bool ends_width_b = str.size() > 1 && std::tolower(str.back()) == 'b' &&
                     std::all_of(str.begin(), str.end() - 1, [](char c){ return c == '0' || c == '1'; });
    // XOR-Prüfung
    return starts_with_b xor ends_width_b;
}

bool Tokenizer::is_hexadecimal(const std::string& str) {
    // Überprüfen, ob der String mit "0x" beginnt xor mit einer ziffer beginnt und mit h endet
    return (str.substr(0, 2) == "0x") xor (str.size() > 1 && std::tolower(str.back()) == 'h' && isdigit(str[0]));
}

// Returns true if the next chars form a valid exponent start like
// 'e' or 'E' followed by either digits or (+|-) and then digits.
bool Tokenizer::is_scientific_exponent_start() const {
	if (peek() != 'e' && peek() != 'E') return false;
	// e<digit>
	if (std::isdigit(peek(1))) return true;
	// e(+|-)<digit>
	if ((peek(1) == '+' || peek(1) == '-') && std::isdigit(peek(2))) return true;
	return false;
}

// Consumes the exponent part:  (e|E)(+|-)?<digits>
// Precondition: is_scientific_exponent_start() already returned true.
void Tokenizer::consume_exponent() {
	// consume 'e' or 'E'
	consume();
	// optional sign
	if (peek() == '+' || peek() == '-') {
		consume();
	}
	// must have at least one digit
	if (!std::isdigit(peek())) {
		auto err_msg = "Malformed scientific literal exponent (missing digits after e/E).";
		CompileError(ErrorType::TokenError, err_msg, m_line, "digits", m_buffer, m_current_file).exit();
	}
	while (std::isdigit(peek())) {
		consume();
	}
}

bool Tokenizer::is_scientific(const std::string& str) {
	// Trim whitespace just in case
	if (str.empty()) return false;

	size_t i = 0;
	bool has_digits = false;
	bool has_dot = false;

	// optional leading sign
	if (str[i] == '+' || str[i] == '-') {
		i++;
		if (i >= str.size()) return false;
	}

	// integer and fractional part
	for (; i < str.size(); ++i) {
		char c = str[i];
		if (std::isdigit(c)) {
			has_digits = true;
			continue;
		} else if (c == '.') {
			if (has_dot) return false; // multiple dots not allowed
			has_dot = true;
			continue;
		}
		break;
	}

	// must have at least one digit before exponent
	if (!has_digits) return false;

	// exponent part required for "scientific"
	if (i >= str.size() || (str[i] != 'e' && str[i] != 'E'))
		return false;
	i++;

	// optional sign after e/E
	if (i < str.size() && (str[i] == '+' || str[i] == '-')) {
		i++;
	}

	// must have at least one digit in exponent
	if (i >= str.size() || !std::isdigit(str[i]))
		return false;

	for (; i < str.size(); ++i) {
		if (!std::isdigit(str[i]))
			return false;
	}

	// all checks passed
	return true;
}

bool Tokenizer::is_integer(const std::string &str) {
	return !str.empty() && std::ranges::all_of(str, ::isdigit);
}

bool Tokenizer::is_callback_start() const {
    if (!m_tokens.empty())
        return CALLBACKS.contains(m_buffer) && m_tokens.back().val == "on";
    return false;
}

bool Tokenizer::is_callback_end() const {
    if (!m_tokens.empty())
        return m_tokens.back().val == "end" && m_buffer == "on";
    return false;
}





