//
// Created by Mathias Vatter on 02.01.24.
//

#include "JSONParser.h"

JSONParser::JSONParser(std::vector<Token> tokens) : Parser(std::move(tokens)) {

}

std::unique_ptr<JSONValue> JSONParser::parse_json() {
    return parse_value();
}

std::unique_ptr<JSONValue> JSONParser::parse_value() {
    auto tok = peek();
    switch (tok.type) {
        case STRING:
            consume();
            return std::make_unique<JSONString>(tok.val);
        case INT:
            consume();
            return std::make_unique<JSONInt>(std::stoi(tok.val));
        case FLOAT:
            consume();
            return std::make_unique<JSONFloat>(std::stod(tok.val));
        case OPEN_CURLY:
            return parse_object();
        case OPEN_BRACKET:
            return parse_array();
        case KEYWORD:
            if(tok.val == "false") {
                return std::make_unique<JSONBool>(false);
            } else if (tok.val == "true") {
                return std::make_unique<JSONBool>(true);
            } else if (tok.val == "null") {
                return nullptr;
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

    while (peek().type != CLOSED_CURLY) {
        auto keyToken = consume(); // Consume key (assumed to be a string)
        if (keyToken.type != STRING) {
            // Handle error
            return nullptr;
        }

        consume(); // Consume ':'
        std::unique_ptr<JSONValue> value = parse_value();
        if (!value) {
            // Handle error
            return nullptr;
        }

        jsonObject->properties[keyToken.val] = std::move(value);

        if (peek().type == COMMA) {
            consume(); // Consume ','
        }
    }

    consume(); // Consume '}'
    return std::move(jsonObject);
}

std::unique_ptr<JSONValue> JSONParser::parse_array() {
    auto current_token = consume(); // Consume '['
    std::unique_ptr<JSONArray> jsonArray = std::make_unique<JSONArray>();

    while (peek().type != CLOSED_BRACKET) {
        std::unique_ptr<JSONValue> value = parse_value();
        if (!value) {
            // Handle error
            return nullptr;
        }

        jsonArray->elements.push_back(std::move(value));

        if (peek().type == COMMA) {
            consume(); // Consume ','
        }
    }

    consume(); // Consume ']'
    return std::move(jsonArray);
}
