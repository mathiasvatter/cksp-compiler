//
// Created by Mathias Vatter on 03.06.23.
//

#pragma once

#include <iostream>
#include <stack>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <optional>

#include "../misc/Result.h"
#include "Tokens.h"
#include "../misc/FileHandler.h"
#include "../misc/FreeFunctions.h"

/*
 * Token struct that gets line numbers, the token type and its value
 */
struct Token {
    token type;
    std::string val;
    size_t line;
    size_t pos;
    std::string file;

    Token() : type(token::INVALID), val(""), line(-1), pos(0), file("") {}
    Token(token type, std::string val, size_t line, size_t pos, const std::string &file);
    friend std::ostream& operator<<(std::ostream& os, const Token& tok);
	bool operator==(const Token &other) const {
		return type == other.type && val == other.val;
	}
};

static std::optional<token> get_token_type(const std::unordered_map<std::string, token>& keywordMap, const std::string& value) {
	if (const auto it = keywordMap.find(value); it != keywordMap.end()) {
        return it->second;
    }
    return std::nullopt;
}



/*
 * Tokenizer Class
 */
class Tokenizer {
public:
	Tokenizer(const std::string& input, const std::string& file, FileType file_type = FileType::ksp);
	~Tokenizer() = default;
	std::vector<Token> tokenize();
	void token_loop();

protected:
    std::string m_input;
    std::string m_current_file;
    size_t m_input_length;
    size_t m_pos;
    char m_current_char;
    size_t m_line;
    size_t m_line_pos;
    std::string m_buffer;
    std::vector<Token> m_tokens;
	bool is_in_fstring = false;
	std::stack<char> fstring_starting_char;

    bool m_is_json = false;
    [[nodiscard]] char peek(int ahead = 0) const;
    char consume();
    void flush_buffer();
	void add_token(token type, std::string val);
	void skip_whitespace();

	static bool is_space(const char& ch);
	[[nodiscard]] bool is_string() const;
    bool is_keyword_or_num() const;

    bool is_pragma() const;
//	void get_pragma();
    void get_line_continuation();
    /// removes linebrk if there was a line_continuation before. Needs to be inserted right after linbrk isnert
    void fix_line_continuation();
    void get_bitwise_operator();
    bool is_callback_end() const;
    bool is_callback_start() const;
    static bool is_hexadecimal(const std::string& str);
    static bool is_binary(const std::string& str);
    void get_comma();
    void get_type();
    void get_linebreak();
    void get_comment();
	void get_invalid();
    void get_comparison_operators();
	void get_compound_assignment_operators();
	void get_string();
    void get_binary_operators();
    void get_parenth();
    void get_assignment();
    void get_arrow();
    void get_keyword_or_num();
    void get_curly_brackets();
	void get_format_string();

};
