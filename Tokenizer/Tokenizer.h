//
// Created by Mathias Vatter on 03.06.23.
//

#pragma once

#include <iostream>
#include <stack>
#include <iomanip>
#include <filesystem>
#include <fstream>

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



inline static long count_char(const std::string& str, char c) {
	return std::count(str.begin(), str.end(), c);
}

inline std::string remove_quotes(const std::string &input) {
	// Überprüfen Sie, ob der String die Mindestlänge hat und ob er mit Anführungszeichen beginnt und endet
	if (input.size() >= 2 &&
		((input.front() == '"' && input.back() == '"') ||
			(input.front() == '\'' && input.back() == '\''))) {
		// Entfernen Sie die Anführungszeichen am Anfang und am Ende
		return input.substr(1, input.size() - 2);
	} else {
		// Wenn keine Anführungszeichen vorhanden sind, geben Sie den ursprünglichen String zurück
		return input;
	}
}


inline static token get_token_type(const std::vector<Keyword>& vec, const std::string& value) {
    auto it = std::find_if(vec.begin(), vec.end(), [&value](const Keyword& kw) {
        return kw.value == value;
    });
    if (it != vec.end()) {
        return it->type;
    }
    return token::INVALID;
}

inline static std::string get_token_value(const std::vector<Keyword>& vec, const token& tok) {
	auto it = std::find_if(vec.begin(), vec.end(),
						   [&tok](const Keyword& kw) {
		return kw.type == tok;
	});

	if (it != vec.end()) {
		return it->value;
	}
	return "";
}

inline static std::optional<token> get_token_type(const std::unordered_map<std::string, token>& keywordMap, const std::string& value) {
    auto it = keywordMap.find(value);
    if (it != keywordMap.end()) {
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

    bool m_is_json = false;
    [[nodiscard]] char peek(int ahead = 0) const;
    char consume();
    void flush_buffer();
	void skip_whitespace();

	static bool is_space(const char& ch);
	[[nodiscard]] bool is_string() const;
    bool is_keyword_or_num() const;

    bool is_pragma();
//	void get_pragma();
    void get_line_continuation();
    /// removes linebrk if there was a line_continuation before. Needs to be inserted right after linbrk isnert
    void fix_line_continuation();
    void get_bitwise_operator();
    bool is_callback_end();
    bool is_callback_start();
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

};
