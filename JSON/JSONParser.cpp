//
// Created by Mathias Vatter on 02.01.24.
//

#include "JSONParser.h"
#include "JSONVisitor.h"


JSONParser::JSONParser(std::vector<Token> tokens) : Parser(std::move(tokens)) {

}

std::unique_ptr<JSONValue> JSONParser::parse_json() {
    return parse_value();
}

std::unique_ptr<JSONValue> JSONParser::parse_value() {
	_skip_linebreaks();
    auto tok = peek();
    switch (tok.type) {
        case token::STRING:
            consume();
            return std::make_unique<JSONString>(tok.val);
		case token::SUB:
			consume();
			tok = peek();
        case token::INT:
            consume();
            return std::make_unique<JSONInt>(std::stoi(tok.val));
        case token::FLOAT:
            consume();
            return std::make_unique<JSONFloat>(std::stod(tok.val));
        case token::OPEN_CURLY:
            return parse_object();
        case token::OPEN_BRACKET:
            return parse_array();
		case token::KEYWORD:
			consume();
            if(tok.val == "false") {
                return std::make_unique<JSONBool>(false);
            } else if (tok.val == "true") {
                return std::make_unique<JSONBool>(true);
            } else if (tok.val == "null") {
                return std::make_unique<JSONObject>();
            }
        default:
            CompileError(ErrorType::ParseError, "Found incorrect json syntax.", tok.line, "", tok.val, tok.file).exit();
            // Handle other cases
            break;
    }
    return nullptr;
}

std::unique_ptr<JSONValue> JSONParser::parse_object() {
    auto current_token = consume(); // Consume '{'
    std::unique_ptr<JSONObject> jsonObject = std::make_unique<JSONObject>();

    while (peek().type != token::CLOSED_CURLY) {
		_skip_linebreaks();
        auto keyToken = consume(); // Consume key (assumed to be a string)
        if (keyToken.type != token::STRING) {
			CompileError(ErrorType::ParseError, "Found incorrect json object syntax.", keyToken.line, "<string>", keyToken.val, keyToken.file).exit();
			// Handle error
            return nullptr;
        }

        consume(); // Consume ':'
        std::unique_ptr<JSONValue> value = parse_value();
        if (!value) {
			CompileError(ErrorType::ParseError, "Found incorrect json object syntax.", keyToken.line, "<value>", keyToken.val, keyToken.file).exit();
            // Handle error
            return nullptr;
        }

        jsonObject->properties[keyToken.val] = std::move(value);

        if (peek().type == token::COMMA) {
            consume(); // Consume ','
        }
		_skip_linebreaks();
    }
    consume(); // Consume '}'
    return std::move(jsonObject);
}

std::unique_ptr<JSONValue> JSONParser::parse_array() {
    auto current_token = consume(); // Consume '['
    std::unique_ptr<JSONArray> jsonArray = std::make_unique<JSONArray>();
    while (peek().type != token::CLOSED_BRACKET) {
		_skip_linebreaks();
        std::unique_ptr<JSONValue> value = parse_value();
        if (!value) {
//			CompileError(ErrorType::ParseError, "Found incorrect json array syntax.", keyToken.line, "<value>", keyToken.val, keyToken.file).exit();
            // Handle error
            return nullptr;
        }

        jsonArray->elements.push_back(std::move(value));

        if (peek().type == token::COMMA) {
            consume(); // Consume ','
        }
		_skip_linebreaks();
    }
    consume(); // Consume ']'
    return std::move(jsonArray);
}

void JSONString::accept(JSONVisitor &visitor) {
	visitor.visit(*this);
}

void JSONInt::accept(JSONVisitor &visitor) {
	visitor.visit(*this);
}

void JSONFloat::accept(JSONVisitor &visitor) {
	visitor.visit(*this);
}

void JSONBool::accept(JSONVisitor &visitor) {
	visitor.visit(*this);
}

void JSONObject::accept(JSONVisitor &visitor) {
	visitor.visit(*this);
}

void JSONArray::accept(JSONVisitor &visitor) {
	visitor.visit(*this);
}
