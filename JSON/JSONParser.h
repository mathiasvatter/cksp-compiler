//
// Created by Mathias Vatter on 02.01.24.
//

#pragma once


#include <utility>

#include "../Parser.h"

struct JSONValue {
    virtual ~JSONValue() = default;
    virtual void accept(class JSONVisitor& visitor) = 0;
};

struct JSONString : JSONValue {
    std::string value;
    inline explicit JSONString(std::string  val) : value(std::move(val)) {}
	void accept(JSONVisitor& visitor) override;
};

struct JSONInt : JSONValue {
    int value;
    inline explicit JSONInt(int val) : value(val) {}
	void accept(JSONVisitor& visitor) override;
};

struct JSONFloat : JSONValue {
    double value;
    inline explicit JSONFloat(double val) : value(val) {}
	void accept(JSONVisitor& visitor) override;
};

struct JSONBool : JSONValue {
    bool value;
    inline explicit JSONBool(bool val) : value(val) {}
	void accept(JSONVisitor& visitor) override;
};


struct JSONObject : JSONValue {
    std::map<std::string, std::unique_ptr<JSONValue>> properties;
	void accept(JSONVisitor& visitor) override;
};

struct JSONArray : JSONValue {
    std::vector<std::unique_ptr<JSONValue>> elements;
	void accept(JSONVisitor& visitor) override;
};


class JSONParser : public Parser {
public:
    explicit JSONParser(std::vector<Token> tokens);

    std::unique_ptr<JSONValue> parse_json();

    std::unique_ptr<JSONValue> parse_value();

    std::unique_ptr<JSONValue> parse_object();

    std::unique_ptr<JSONValue> parse_array();

};


