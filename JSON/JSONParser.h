//
// Created by Mathias Vatter on 02.01.24.
//

#pragma once


#include <utility>

#include "../Parser.h"
#include "JSONVisitor.h"

struct JSONValue {
    virtual ~JSONValue() = default;
    virtual void accept(class JSONVisitor& visitor) = 0;
};

struct JSONString : JSONValue {
    std::string value;
    inline explicit JSONString(std::string  val) : value(std::move(val)) {}
    inline void accept(JSONVisitor& visitor) override {
        visitor.visit(*this);
    }
};

struct JSONInt : JSONValue {
    int value;
    inline explicit JSONInt(int val) : value(val) {}
    inline void accept(JSONVisitor& visitor) override {
        visitor.visit(*this);
    }
};

struct JSONFloat : JSONValue {
    double value;
    inline explicit JSONFloat(double val) : value(val) {}
    inline void accept(JSONVisitor& visitor) override {
        visitor.visit(*this);
    }
};

struct JSONBool : JSONValue {
    bool value;
    inline explicit JSONBool(bool val) : value(val) {}
    inline void accept(JSONVisitor& visitor) override {
        visitor.visit(*this);
    }
};


struct JSONObject : JSONValue {
    std::map<std::string, std::unique_ptr<JSONValue>> properties;
    inline void accept(JSONVisitor& visitor) override {
        visitor.visit(*this);
    }
};

struct JSONArray : JSONValue {
    std::vector<std::unique_ptr<JSONValue>> elements;
    inline void accept(JSONVisitor& visitor) override {
        visitor.visit(*this);
    }
};


class JSONParser : public Parser {
public:
    explicit JSONParser(std::vector<Token> tokens);

    std::unique_ptr<JSONValue> parse_json();

    std::unique_ptr<JSONValue> parse_value();

    std::unique_ptr<JSONValue> parse_object();

    std::unique_ptr<JSONValue> parse_array();

};


