//
// Created by Mathias Vatter on 03.06.23.
//

#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <algorithm>
#include <stack>
#include <cmath>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../Result.h"
#include "Tokens.h"
#include "../FileHandler.h"

/*
 * Token struct that gets line numbers, the token type and its value
 */
struct Token {
    token type;
    std::string val;
    size_t line;
    size_t pos;
    std::string file;

    Token() : type(token::INVALID), val(""), line(0), pos(0), file("") {}
    Token(token type, std::string val, size_t line, size_t pos, const std::string &file);
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
inline static bool contains(std::set<char> &vec, char c) {
    return vec.find(c) != vec.end();
};
inline static bool contains(const std::vector<std::string>& vec, const std::string& value) {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
};
inline static bool contains(const std::set<std::string>& vec, const std::string& value) {
    return vec.find(value) != vec.end();
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

inline std::string remove_substring(const std::string& str, const std::string& substring) {
    // Suche nach dem Substring im Hauptstring
    size_t pos = str.find(substring);
    // Wenn der Substring gefunden wurde, entferne ihn
    if (pos != std::string::npos) {
        auto new_str = str;
        new_str.erase(pos, substring.length());
        return new_str;
    }
    return str;
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

/// takes string and returns vector of namespaces ('.')
inline std::vector<std::string> get_namespaces(const std::string& str) {
	std::vector<std::string> namespaces;
	std::istringstream iss(str);
	std::string ns;

	while (std::getline(iss, ns, '.')) {
		namespaces.push_back(ns);
	}
	return namespaces;
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


// Struktur für den zusammengesetzten Schlüssel
struct StringIntKey {
    std::string str;
    int num;

    bool operator==(const StringIntKey& other) const {
        return str == other.str && num == other.num;
    }
};
// Benutzerdefinierte Hash-Funktion
struct StringIntKeyHash {
    std::size_t operator()(const StringIntKey& key) const {
        return std::hash<std::string>()(key.str) ^ std::hash<int>()(key.num);
    }
};

static std::string read_file(const std::string& filename);

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
	void get_string();
    void get_binary_operators();
    void get_parenth();
    void get_assignment();
    void get_arrow();
    void get_keyword_or_num();
    void get_curly_brackets();

};
