//
// Created by Mathias Vatter on 09.05.25.
//

#pragma once

#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <memory>

class JSONVisitor;

struct JSONValue {
	virtual ~JSONValue() = default;
	virtual void accept(JSONVisitor& visitor) {};
	std::string get_string();
};

struct JSONString : JSONValue {
	std::string value;
	explicit JSONString(std::string  value) : value(std::move(value)) {}
	void accept(JSONVisitor& visitor) override;
};

struct JSONInt : JSONValue {
	long long value;
	explicit JSONInt(const long long value) : value(value) {}
	void accept(JSONVisitor& visitor) override;
};

struct JSONFloat : JSONValue {
	double value;
	explicit JSONFloat(const double value) : value(value) {}
	void accept(JSONVisitor& visitor) override;
};

struct JSONBool : JSONValue {
	bool value;
	explicit JSONBool(const bool value) : value(value) {}
	void accept(JSONVisitor& visitor) override;
};

struct JSONNull : JSONValue {
	JSONNull() = default;
	void accept(JSONVisitor& visitor) override;
};

struct JSONObject : JSONValue {
	std::vector<std::pair<std::string, std::unique_ptr<JSONValue>>> properties;

	void add(const std::string& key, std::unique_ptr<JSONValue> value);
	void accept(JSONVisitor& visitor) override;
};

struct JSONArray : JSONValue {
	std::vector<std::unique_ptr<JSONValue>> elements;

	// Generischer variadischer Konstruktor für unique_ptr<JSONValue>
	template<typename... Args, std::enable_if_t<(std::is_same_v<std::unique_ptr<JSONValue>, std::decay_t<Args>> && ...), int> = 0>
	explicit JSONArray(Args&&... args) {
		(add(std::forward<Args>(args)), ...);
	}

	// Variadischer Konstruktor speziell für Strings
	template<typename... Strings, std::enable_if_t<(std::is_convertible_v<Strings, std::string> && ...), int> = 0>
	explicit JSONArray(Strings&&... strs) {
		(add(std::make_unique<JSONString>(std::forward<Strings>(strs))), ...);
	}

	explicit JSONArray() = default;

	void add(std::unique_ptr<JSONValue> value);
	void accept(JSONVisitor& visitor) override;
};