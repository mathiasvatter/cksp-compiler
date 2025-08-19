//
// Created by Mathias Vatter on 14.05.25.
//

#pragma once

#include <iostream>

/**
 * Defines the token types used in the JSON parser.
 * Defines a Token struct that represents a token in the JSON file including debugging information.
**/

/// defines the token names and the string that represents them while debugging
#define JSON_ENUM_LIST(XX) \
	XX(INVALID, "invalid") \
	XX(END_TOKEN, "end_of_file") \
	XX(LINEBRK, "linebreak") \
	XX(INT, "int") \
	XX(FLOAT, "float") \
	XX(MINUS, "minus") \
	XX(HEXADECIMAL, "hexadecimal") \
	XX(BINARY, "binary") \
	XX(COMMENT, "comment") \
    XX(STRING, "string") \
    XX(COMMA, "comma sep") \
	XX(KEYWORD, "Keyword") \
	XX(COLON, "colon") \
    XX(OPEN_BRACKET, "open_bracket")\
    XX(CLOSED_BRACKET, "closed_bracket") \
    XX(OPEN_CURLY, "closed_curly") \
    XX(CLOSED_CURLY, "closed_curly") \
    XX(NUL, "null") \
	XX(TRUE, "true") \
	XX(FALSE, "false")


#define JSON_ENUM(name, str) name,
enum class jtoken {
    JSON_ENUM_LIST(JSON_ENUM)
};
#undef JSON_ENUM


#define STRING(name, str) str,
inline const char *jtoken_strings[] = {
    JSON_ENUM_LIST(STRING)
};
#undef STRING

/// overwrite the << operator to make debugging easier
inline std::ostream &operator<<(std::ostream &os, const jtoken &tok) {
    os << jtoken_strings[static_cast<int>(tok)];
    return os;
}

inline std::unordered_map<char, jtoken> JSON_PUNCTUATION = {
	{'[', jtoken::OPEN_BRACKET},
	{']', jtoken::CLOSED_BRACKET},
	{'{', jtoken::OPEN_CURLY},
	{'}', jtoken::CLOSED_CURLY},
	{':', jtoken::COLON},
	{',', jtoken::COMMA},
};

inline std::optional<jtoken> get_json_keyword_token(const std::string& value) {
	std::unordered_map<std::string, jtoken> JSON_KEYWORDS = {
		{"false", jtoken::FALSE},
		{"true", jtoken::TRUE},
		{"null", jtoken::NUL},
	};
	if (auto const it = JSON_KEYWORDS.find(value); it != JSON_KEYWORDS.end()) {
		return it->second;
	}
	return std::nullopt;
}

struct JSONToken {
	jtoken type = jtoken::INVALID;
	std::string val;
	size_t line = -1;
	size_t pos = 0;
	std::string file;

	JSONToken() = default;
	JSONToken(jtoken type, std::string val, size_t line, size_t pos, std::string file)
	: type(type), val(std::move(val)), line(line), pos(pos), file(std::move(file)) {}
	friend std::ostream& operator<<(std::ostream& os, const JSONToken& tok);
	bool operator==(const JSONToken &other) const {
		return type == other.type && val == other.val;
	}
};

// Definition of the operator<< for struct Token
// Marked inline because it's defined in a header file.
inline std::ostream& operator<<(std::ostream& os, const JSONToken& tok) {
	os << "Token(type: " << tok.type
	   << ", val: \"" << tok.val << "\""
	   << ", line: " << tok.line
	   << ", pos: " << tok.pos
	   << ", file: \"" << tok.file << "\")";
	return os;
}