//
// Created by Mathias Vatter on 13.05.25.
//

#pragma once

#include <utility>
#include "JSONTokens.h"
#include "../error/JSONError.h"

class JSONTokenizer {
	std::string m_input;
	std::string m_current_file;
	size_t m_input_length;
	size_t m_pos = 0;
	// char m_current_char;
	size_t m_line = 1;
	size_t m_line_pos = 0;
	std::string m_buffer;
	std::vector<JSONToken> m_tokens;

public:
	JSONTokenizer(std::string input, std::string file)
		: m_input(std::move(input)), m_current_file(std::move(file)) {
		m_input += '\n'; // Ensure the last line is processed
		m_input_length = m_input.size();
	}

	~JSONTokenizer() = default;

	std::vector<JSONToken> tokenize() {
		m_tokens.clear(); // Clear previous tokens if tokenize is called multiple times
		m_pos = 0;
		m_line = 1;
		m_line_pos = 0;
		if(m_input.empty()) {
			add_token(jtoken::END_TOKEN);
			return m_tokens;
		}
		while (m_pos < m_input_length-1) {
			char ch = peek();
			if (JSON_PUNCTUATION.contains(ch)) {
				get_punctuation();
			} else if (is_keyword_or_num()) {
				get_keyword_or_num();
			} else if (is_string_start()) {
				get_string();
			} else if (peek() == '-') {
				get_minus();
			} else if(is_whitespace(peek())) {
				skip_whitespace_and_count_lines();
			} else {
				get_invalid();
			}
		}
		add_token(jtoken::END_TOKEN);
		return m_tokens;
	}

private:
	[[nodiscard]] char peek(const int ahead = 0) const {
		if (m_input_length <= m_pos + ahead) {
			throw JSONParseError("Reached end of input",
				m_current_file,
				m_line,
				m_pos,
				"end token",
				m_buffer
			);
		}
		return m_input.at(m_pos + ahead);
	}
	char consume() {
		if (m_pos < m_input_length) {
			m_buffer += peek();
			m_line_pos++;
			return m_input.at(m_pos++);
		}
		throw JSONParseError("Reached end of file.",
			m_current_file,
			m_line,
			m_pos,
			"end token",
			std::string(1, peek())
		);
	}

	void flush_buffer() {
		m_buffer.clear();
	}

	void skip_whitespace_and_count_lines() {
		while (m_pos < m_input_length && is_whitespace(peek())) {
			if (peek() == '\n') {
				m_line++;
				m_line_pos = 0;
			} else {
				m_line_pos++;
			}
			m_pos++;
		}
		flush_buffer();
	}

	static bool is_whitespace(const char &ch) {
		return ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r' || ch == ' ';
	}

	void add_token(jtoken type) {
		m_tokens.emplace_back(type, m_buffer, m_line, m_line_pos - m_buffer.length(), m_current_file);
		flush_buffer();
	}

	void get_invalid() {
		flush_buffer();
		consume();
		add_token(jtoken::INVALID);
		throw JSONParseError("Invalid character.",
			m_current_file,
			m_line,
			m_pos,
			"invalid token",
			m_buffer
		);
	}

	[[nodiscard]] bool is_string_start() const {
		return peek() == '\'' || peek() == '"';
	}

	void get_string() {
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
		if (m_buffer.length() > 1) {
			m_buffer = m_buffer.substr(1, m_buffer.length()-2);
		}
		add_token(jtoken::STRING);
	}

	void get_punctuation() {
		flush_buffer();
		char p = consume();
		add_token(JSON_PUNCTUATION.at(p));
	}


	void get_minus() {
		flush_buffer();
		consume(); // consume -
		add_token(jtoken::MINUS);
	}

	[[nodiscard]] bool is_keyword_or_num() const {
		return std::isalnum(peek()) || peek() == '_' || peek() == '.';
	}

	void get_keyword_or_num() {
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
	            add_token(jtoken::FLOAT);
	        } else {
	        	throw JSONParseError("Found invalid number.",
	        		m_current_file,
	        		m_line,
	        		m_pos,
	        		"valid number",
	        		m_buffer
	        	);
			}
	    // check if next char is _ or text
	    } else if (is_keyword_or_num()) {
	        consume(); //consume possible identifier
	        while (std::isalnum(peek()) || peek() == '_') {
	            consume();
	        }
			while (peek() == '.') {
				consume();
				if (std::isalnum(peek()) || peek() == '_') {
					while(std::isalnum(peek()) || peek() == '_') {
						consume();
					}
				} else {
					throw JSONParseError("Found invalid number or keyword.",
						m_current_file,
						m_line,
						m_pos,
						"valid number or keyword",
						m_buffer
					);
				}
			}
	        jtoken tok;
	    	if (auto type = get_json_keyword_token(m_buffer)) {
	            add_token(*type);
	        }
	    } else // is probably int
	        add_token(jtoken::INT);
	}


};