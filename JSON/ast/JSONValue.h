//
// Created by Mathias Vatter on 09.05.25.
//

#pragma once

#include <map>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <memory>

class JSONVisitor;

struct JSONValue {
	JSONValue() = default;
	virtual ~JSONValue() = default;
	JSONValue(const JSONValue& other) = default;
	JSONValue& operator=(const JSONValue& other) = default;
	virtual void accept(JSONVisitor& visitor) = 0;
	virtual std::unique_ptr<JSONValue> clone() const = 0;
	std::string get_string();

	template <typename T>
	[[nodiscard]] T* as() {
		static_assert(std::is_base_of_v<JSONValue, T>);
		return dynamic_cast<T*>(this);
	}

	template <typename T>
	[[nodiscard]] const T* as() const {
		static_assert(std::is_base_of_v<JSONValue, T>);
		return dynamic_cast<const T*>(this);
	}
};

struct JSONString : JSONValue {
	std::string value;
	explicit JSONString(std::string  value) : value(std::move(value)) {}
	void accept(JSONVisitor& visitor) override;
	std::unique_ptr<JSONValue> clone() const override;
};

struct JSONInt : JSONValue {
	long long value;
	explicit JSONInt(const long long value) : value(value) {}
	void accept(JSONVisitor& visitor) override;
	std::unique_ptr<JSONValue> clone() const override;
};

struct JSONFloat : JSONValue {
	double value;
	explicit JSONFloat(const double value) : value(value) {}
	void accept(JSONVisitor& visitor) override;
	std::unique_ptr<JSONValue> clone() const override;
};

struct JSONBool : JSONValue {
	bool value;
	explicit JSONBool(const bool value) : value(value) {}
	void accept(JSONVisitor& visitor) override;
	std::unique_ptr<JSONValue> clone() const override;
};

struct JSONNull : JSONValue {
	JSONNull() = default;
	void accept(JSONVisitor& visitor) override;
	std::unique_ptr<JSONValue> clone() const override;
};

struct JSONObject : JSONValue {
	std::map<std::string, std::unique_ptr<JSONValue>> properties;

	JSONObject() = default;
	JSONObject(const JSONObject& other);
	JSONObject& operator=(const JSONObject& other);
	void add(const std::string& key, std::unique_ptr<JSONValue> value);
	/// get value
	const JSONValue* get(const std::string& key) const;
	/// get value and cast it at the same time
	template <typename T>
	[[nodiscard]] const T* get(const std::string& key) const {
		const auto* value = get(key);
		return value ? value->as<T>() : nullptr;
	}
	bool contains(const std::string& key) const;
	void accept(JSONVisitor& visitor) override;
	std::unique_ptr<JSONValue> clone() const override;
};

struct JSONArray : JSONValue {
	std::vector<std::unique_ptr<JSONValue>> elements;

	// variadic constructor for JSONValue unique_ptr objects
	template<typename... Args, std::enable_if_t<(std::is_same_v<std::unique_ptr<JSONValue>, std::decay_t<Args>> && ...), int> = 0>
	explicit JSONArray(Args&&... args) {
		(add(std::forward<Args>(args)), ...);
	}

	// variadic cosntructor for std::string objects
	template<typename... Strings, std::enable_if_t<(std::is_convertible_v<Strings, std::string> && ...), int> = 0>
	explicit JSONArray(Strings&&... strs) {
		(add(std::make_unique<JSONString>(std::forward<Strings>(strs))), ...);
	}

	explicit JSONArray() = default;
	JSONArray(const JSONArray& other);
	JSONArray& operator=(const JSONArray& other);
	[[nodiscard]] size_t size() const;
	const JSONValue* at(size_t index) const;
	void add(std::unique_ptr<JSONValue> value);
	void accept(JSONVisitor& visitor) override;
	std::unique_ptr<JSONValue> clone() const override;
};
