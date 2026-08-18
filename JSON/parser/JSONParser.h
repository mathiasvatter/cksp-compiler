//
// Created by Mathias Vatter on 14.05.25.
//

#pragma once

#include <cctype>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "JSONTokenizer.h"
#include "../ast/JSONValue.h"

class JSONParser {
	size_t m_pos = 0;
	std::vector<JSONToken> m_tokens{};
	std::string m_curr_token_value{};
	JSONToken m_curr_token;
	jtoken m_curr_token_type = jtoken::INVALID;

public:
	explicit JSONParser() = default;
	explicit JSONParser(std::vector<JSONToken> tokens) : m_tokens(std::move(tokens)) {}

	static std::unique_ptr<JSONValue> parse(std::string json, std::string file = "<string>") {
		JSONTokenizer tokenizer(std::move(json), std::move(file));
		JSONParser parser(tokenizer.tokenize());
		return parser.parse();
	}

	std::unique_ptr<JSONValue> parse() {
		m_pos = 0;
		auto value = parse_value();
		const auto tok = peek();
		if (tok.type != jtoken::END_TOKEN) {
			throw JSONParseError("Found content after valid json value.",
				"end token",
				tok
			);
		}
		return value;
	}

private:
	std::unique_ptr<JSONValue> parse_value() {
		auto tok = peek();
		switch (tok.type) {
			case jtoken::STRING:
				consume();
				return std::make_unique<JSONString>(unescape_string(tok.val));
			case jtoken::MINUS:
			case jtoken::INT:
			case jtoken::FLOAT:
				return parse_number();
			case jtoken::OPEN_CURLY:
				return parse_object();
			case jtoken::OPEN_BRACKET:
				return parse_array();
			case jtoken::NUL:
				consume();
				return std::make_unique<JSONNull>();
			case jtoken::BOOLEAN_TRUE:
				consume();
				return std::make_unique<JSONBool>(true);
			case jtoken::BOOLEAN_FALSE:
				consume();
				return std::make_unique<JSONBool>(false);
			default:
				throw JSONParseError("Found incorrect json syntax.",
					"valid json value",
					tok
				);
		}
	}

	std::unique_ptr<JSONValue> parse_object() {
		auto current_token = consume(); // Consume '{'
		auto json_object = std::make_unique<JSONObject>();

		while (peek().type != jtoken::CLOSED_CURLY) {
			auto key_token = consume(); // Consume key (assumed to be a string)
			if (key_token.type != jtoken::STRING) {
				throw JSONParseError("Found incorrect <json object> syntax.",
					"valid <json key>",
					key_token
				);
			}
			if (peek().type != jtoken::COLON) {
				throw JSONParseError("Expected ':' after key in JSON object.",
				   ":",
				   peek()
				);
			}
			consume(); // Consume ':'
			std::unique_ptr<JSONValue> value = parse_value();
			if (!value) {
				throw JSONParseError("Found incorrect <json object> syntax.",
					"valid <json value>",
					key_token
				);
			}

			json_object->add(key_token.val, std::move(value));

			if (peek().type == jtoken::COMMA) {
				consume(); // Consume ','
			}
		}
		consume(); // Consume '}'
		return std::move(json_object);
	}

	std::unique_ptr<JSONValue> parse_array() {
		auto current_token = consume(); // Consume '['
		auto json_array = std::make_unique<JSONArray>();
		while (peek().type != jtoken::CLOSED_BRACKET) {
			std::unique_ptr<JSONValue> value = parse_value();
			if (!value) {
				throw JSONParseError("Found incorrect <json array> syntax.",
					"valid <json value>",
					m_curr_token
				);
			}

			json_array->add(std::move(value));

			if (peek().type == jtoken::COMMA) {
				consume(); // Consume ','
			}
		}
		consume(); // Consume ']'
		return std::move(json_array);
	}


private:
    std::unique_ptr<JSONValue> parse_number() {
	    bool is_negative = false;

    	if (peek().type == jtoken::MINUS) {
    		consume(); // Consume the MINUS token
    		is_negative = true;
    	}

    	auto number_token = peek();
    	if (number_token.type != jtoken::INT && number_token.type != jtoken::FLOAT) {
    		throw JSONParseError("Expected INT or FLOAT token after optional MINUS sign.",
							 "INT or FLOAT token",
							 number_token
				);
    	}

    	consume(); // Consume the INT or FLOAT token itself

    	std::string val_str = number_token.val;
    	std::string final_val_str = is_negative ? ("-" + val_str) : val_str;

    	// JSON number format validation
    	// 1. Leading zeros: "01" is invalid, "0" is valid. "-01" is invalid.
    	if (val_str.length() > 1 && val_str[0] == '0' && val_str.find('.') == std::string::npos) { // Check for non-float leading zero
    		throw JSONParseError("Invalid number format: leading zero not allowed for integers (unless number is 0).",
							number_token.file, (long long)number_token.line, number_token.pos,
							"no leading zero (e.g. '0' or '123')", final_val_str);
    	}
    	if (is_negative && val_str.length() > 1 && val_str[0] == '0' && val_str.find('.') == std::string::npos) {
    		throw JSONParseError("Invalid number format: leading zero after minus not allowed for integers (unless number is 0).",
							number_token.file, (long long)number_token.line, number_token.pos,
							"no leading zero after minus (e.g. '-0' or '-123')", final_val_str);
    	}


    	if (number_token.type == jtoken::FLOAT) {
    		try {
    			double val = std::stod(final_val_str);
    			return std::make_unique<JSONFloat>(val);
    		} catch (...) {
    			throw JSONParseError("Unable to parse float value: '" + final_val_str + "'.",
								 number_token.file, (long long)number_token.line, number_token.pos,
								 "valid float digits/format", final_val_str);
    		}
    	} else { // Treat as integer
    		try {
    			// JSON integers can be large, consider std::stoll if JSONInt stores long long
    			// For now, assuming JSONInt takes int.
    			long long long_val = std::stoll(final_val_str); // Parse as long long first
    			// Check if it fits into int if JSONInt stores int
    			if (long_val < std::numeric_limits<int>::min() || long_val > std::numeric_limits<int>::max()) {
    				throw JSONParseError("Integer value '" + final_val_str + "' out of range for int.",
									number_token.file, (long long)number_token.line, number_token.pos,
									"integer within int range", final_val_str);
    			}
    			return std::make_unique<JSONInt>(static_cast<int>(long_val));
    		} catch (...) {
    			throw JSONParseError("Unable to parse integer: '" + final_val_str + "'.",
								 number_token.file, (long long)number_token.line, number_token.pos,
								 "valid integer digits", final_val_str);
    		}
    	}
    }

	JSONToken peek(const int ahead = 0) {
		if (m_pos + ahead >= m_tokens.size()) {
			const auto token = m_tokens.empty() ? JSONToken{} : m_tokens.back();
			throw JSONParseError("Reached the end of the tokens. Wrong Syntax discovered.",
				"end token",
				token
			);
		}
		m_curr_token = m_tokens.at(m_pos + ahead);
		m_curr_token_type = m_curr_token.type;
		m_curr_token_value = m_curr_token.val;
		return m_curr_token;
	}

	JSONToken consume() {
		if (m_pos >= m_tokens.size()) {
			const auto token = m_tokens.empty() ? JSONToken{} : m_tokens.back();
			throw JSONParseError("Reached the end of the tokens. Wrong Syntax discovered.",
				"end token",
				token
			);
		}
		const auto consumed = m_tokens.at(m_pos++);
		m_curr_token = m_pos < m_tokens.size() ? m_tokens.at(m_pos) : consumed;
		m_curr_token_type = m_curr_token.type;
		m_curr_token_value = m_curr_token.val;
		return consumed;
	}

	/// The codepoint of a <\uXXXX> escape at `position` (the backslash), and where it ends.
	///
	/// A codepoint outside the basic plane is written as two escapes, a high and a low
	/// surrogate, which only mean anything together - so both are read here or neither is.
	static std::optional<std::pair<char32_t, size_t>> decode_unicode_escape(
		const std::string& s, const size_t position) {
		const auto hex_at = [&s](const size_t start) -> std::optional<char32_t> {
			if (start + 4 > s.length()) return std::nullopt;
			char32_t value = 0;
			for (size_t index = start; index < start + 4; ++index) {
				const auto digit = static_cast<unsigned char>(s[index]);
				if (!std::isxdigit(digit)) return std::nullopt;
				value = value * 16 + (std::isdigit(digit)
					? digit - '0'
					: (std::tolower(digit) - 'a' + 10));
			}
			return value;
		};

		const auto first = hex_at(position + 2);
		if (!first) return std::nullopt;
		size_t end = position + 6;
		char32_t codepoint = *first;
		if (codepoint >= 0xD800 && codepoint <= 0xDBFF
			&& end + 1 < s.length() && s[end] == '\\' && s[end + 1] == 'u') {
			if (const auto low = hex_at(end + 2); low && *low >= 0xDC00 && *low <= 0xDFFF) {
				codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (*low - 0xDC00);
				end += 6;
			}
		}
		return std::pair{codepoint, end};
	}

	/// Writes `codepoint` to `out` as UTF-8, which is the encoding the rest of the compiler
	/// reads its input in.
	static void append_utf8(std::string& out, const char32_t codepoint) {
		if (codepoint < 0x80) {
			out += static_cast<char>(codepoint);
		} else if (codepoint < 0x800) {
			out += static_cast<char>(0xC0 | (codepoint >> 6));
			out += static_cast<char>(0x80 | (codepoint & 0x3F));
		} else if (codepoint < 0x10000) {
			out += static_cast<char>(0xE0 | (codepoint >> 12));
			out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (codepoint & 0x3F));
		} else {
			out += static_cast<char>(0xF0 | (codepoint >> 18));
			out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
			out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (codepoint & 0x3F));
		}
	}

	static std::string unescape_string(const std::string& s) {
    	std::string result;
    	result.reserve(s.length()); // Kleine Optimierung

    	for (size_t i = 0; i < s.length(); ++i) {
    		if (s[i] == '\\' && i + 1 < s.length()) {
    			// Ein Backslash wurde gefunden, schaue auf das nächste Zeichen
    			char next_char = s[i + 1];
    			switch (next_char) {
    				case '"':  result += '"';  break;
    				case '\\': result += '\\'; break;
    				case '/':  result += '/';  break;
    				case 'b':  result += '\b'; break;
    				case 'f':  result += '\f'; break;
    				case 'n':  result += '\n'; break;
    				case 'r':  result += '\r'; break;
    				case 't':  result += '\t'; break;
    				case 'u': {
    					// A client is free to send any character escaped, and some send every
    					// non-ASCII one that way. Dropping the escape put a literal backslash
    					// into the document text, so the compiler tokenized something the user
    					// never wrote - and no test could carry a non-ASCII character at all.
    					const auto decoded = decode_unicode_escape(s, i);
    					if (!decoded) {
    						result += '\\';
    						break;
    					}
    					append_utf8(result, decoded->first);
    					i = decoded->second - 1; // the loop's ++i steps past the last digit
    					continue;
    				}
    				default:
    					// Falls eine ungültige Sequenz wie z.B. "\q" auftritt.
    					// Man könnte hier einen Fehler werfen oder die Sequenz ignorieren.
    					// Wir fügen einfach den Backslash selbst hinzu.
    					result += '\\';
    					break;
    			}
    			i++; // Wichtig: Überspringe das Zeichen nach dem Backslash
    		} else {
    			// Ein normales Zeichen, einfach hinzufügen
    			result += s[i];
    		}
    	}
    	return result;
    }

};
