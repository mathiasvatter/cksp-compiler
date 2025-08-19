//
// Created by Mathias Vatter on 14.05.25.
//

#pragma once

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

	std::unique_ptr<JSONValue> parse() {
		return parse_value();
	}

	std::unique_ptr<JSONValue> parse(const std::string& json, std::string file) {
		static JSONTokenizer tokenizer(json, std::move(file));
		m_tokens = tokenizer.tokenize();
		m_pos = 0;
		return JSONParser::parse();
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
			case jtoken::TRUE:
				consume();
				return std::make_unique<JSONBool>(true);
			case jtoken::FALSE:
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
		m_curr_token = m_tokens.at(m_pos);
		m_curr_token_type = m_curr_token.type;
		m_curr_token_value = m_curr_token.val;
		if (m_tokens.size() < m_pos+ahead) {
			throw JSONParseError("Reached the end of the tokens. Wrong Syntax discovered.",
				"end token",
				m_curr_token
			);
		}
		return m_tokens.at(m_pos+ahead);
	}

	JSONToken consume() {
		m_curr_token = m_tokens.at(m_pos+1);
		m_curr_token_type = m_curr_token.type;
		m_curr_token_value = m_curr_token.val;
		if (m_pos >= m_tokens.size()) {
			throw JSONParseError("Reached the end of the tokens. Wrong Syntax discovered.",
				"end token",
				m_curr_token
			);
		}
		return m_tokens.at(m_pos++);
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
    					// TODO: Unicode-Sequenzen (\uXXXX) könnten hier noch hinzugefügt werden
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