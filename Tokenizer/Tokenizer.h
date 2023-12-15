//
// Created by Mathias Vatter on 03.06.23.
//

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <unordered_map>

#include "../Result.h"
#include "Tokens.h"


/*
 * Token struct that gets m_line numbers, the token type and its value
 */
struct Token {
    token type;
    std::string val;
    size_t line;
    std::string file;

    Token() : type(token::INVALID), val(""), line(0), file("") {}
    Token(token type, std::string val, size_t line, std::string &file);
    friend std::ostream& operator<<(std::ostream& os, const Token& tok);
	bool operator==(const Token &other) const {
		return type == other.type && val == other.val;
	}
};

/*
 * helper function to search vectors for chars, std::String and Keyword obj
 */
inline static bool contains(std::vector<char> &vec, char c) {
    return std::any_of(vec.begin(), vec.end(), [&](const auto& ch) {return ch == c;});
};
inline static bool contains(const std::vector<std::string>& vec, const std::string& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
};
inline static bool contains(const std::vector<Keyword>& vec, const std::string& value) {
    return std::find_if(vec.begin(), vec.end(), [&value](const Keyword& kw) {
        return kw.value == value;
    }) != vec.end();
};
inline static bool contains(const std::string& string, const std::string& substring) {
	return string.find(substring) != std::string::npos;
}

inline static long count_char(const std::string& str, char c) {
	return std::count(str.begin(), str.end(), c);
}

inline static token get_token_type(const std::vector<Keyword>& vec, const std::string& value) {
    auto it = std::find_if(vec.begin(), vec.end(), [&value](const Keyword& kw) {
        return kw.value == value;
    });
    if (it != vec.end()) {
        return it->type;
    }
    return INVALID;
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

inline static std::string to_lower(const std::string& input) {
    std::string output = input;
    std::transform(output.begin(), output.end(), output.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return output;
}

inline static std::string to_upper(const std::string& input) {
    std::string output = input;
    std::transform(output.begin(), output.end(), output.begin(),
                   [](unsigned char c) { return std::toupper(c); });
    return output;
}

inline bool string_compare(const std::string& str1, const std::string& str2) {
    if (str1.length() != str2.length()) {
        return false;
    }
    for (size_t i = 0; i < str1.length(); ++i) {
        if (std::tolower(str1[i]) != std::tolower(str2[i])) {
            return false;
        }
    }
    return true;
}

/*
 * Tokenizer Class
 */
class Tokenizer {

private:
    std::string m_input;
    std::string m_current_file;
    size_t m_input_length;
    size_t m_pos;
    char m_current_char;
    size_t m_line;
    std::string m_buffer;
    std::vector<Token> m_tokens;

    [[nodiscard]] char peek(int ahead = 0) const;
    char consume();
    void flush_buffer();
	void skip_whitespace();

    static std::string read_file(const std::string& filename);

	static bool is_space(const char& ch);
	[[nodiscard]] bool is_string() const;
    bool is_keyword_or_num() const;

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
	void get_string();
    void get_binary_operators();
    void get_parenth();
    void get_assignment();
    void get_arrow();
    void get_keyword_or_num();

public:
    explicit Tokenizer(std::string file);
    ~Tokenizer() = default;
    std::vector<Token> tokenize();
};
