//
// Created by Mathias Vatter on 09.05.25.
//

#include "JSONValue.h"

#include "../visitor/JSONPrintVisitor.h"
#include "../visitor/JSONVisitor.h"

std::string JSONValue::get_string() const {
	JSONPrintVisitor visitor;
	return visitor.get_string(*this);
}

void JSONString::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONString::accept(JSONPrintVisitor& visitor) const {
	visitor.visit(*this);
}

std::unique_ptr<JSONValue> JSONString::clone() const {
	return std::make_unique<JSONString>(*this);
}

void JSONInt::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONInt::accept(JSONPrintVisitor& visitor) const {
	visitor.visit(*this);
}

std::unique_ptr<JSONValue> JSONInt::clone() const {
	return std::make_unique<JSONInt>(*this);
}

void JSONFloat::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONFloat::accept(JSONPrintVisitor& visitor) const {
	visitor.visit(*this);
}

std::unique_ptr<JSONValue> JSONFloat::clone() const {
	return std::make_unique<JSONFloat>(*this);
}

void JSONBool::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONBool::accept(JSONPrintVisitor& visitor) const {
	visitor.visit(*this);
}

std::unique_ptr<JSONValue> JSONBool::clone() const {
	return std::make_unique<JSONBool>(*this);
}

void JSONNull::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONNull::accept(JSONPrintVisitor& visitor) const {
	visitor.visit(*this);
}

std::unique_ptr<JSONValue> JSONNull::clone() const {
	return std::make_unique<JSONNull>(*this);
}

JSONObject::JSONObject(const JSONObject& other) : JSONValue(other) {
	for (const auto& [key, value] : other.properties) {
		properties.emplace(key, value ? value->clone() : nullptr);
	}
}

JSONObject& JSONObject::operator=(const JSONObject& other) {
	if (this == &other) {
		return *this;
	}

	std::map<std::string, std::unique_ptr<JSONValue>> copied_properties;
	for (const auto& [key, value] : other.properties) {
		copied_properties.emplace(key, value ? value->clone() : nullptr);
	}

	properties = std::move(copied_properties);
	return *this;
}

void JSONObject::add(const std::string &key, std::unique_ptr<JSONValue> value) {
	const auto it = properties.find(key);
	// if key exists, update its value
	if (it != properties.end()) {
		it->second = std::move(value);
		return;
	}
	properties.emplace(key, std::move(value));
}

const JSONValue * JSONObject::get(const std::string &key) const {
	const auto it = properties.find(key);
	if (it != properties.end()) {
		return it->second.get();
	}
	return nullptr;
}

bool JSONObject::contains(const std::string &key) const {
	return properties.contains(key);
}

void JSONObject::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONObject::accept(JSONPrintVisitor& visitor) const {
	visitor.visit(*this);
}

std::unique_ptr<JSONValue> JSONObject::clone() const {
	return std::make_unique<JSONObject>(*this);
}

JSONArray::JSONArray(const JSONArray& other) : JSONValue(other) {
	elements.reserve(other.elements.size());
	for (const auto& value : other.elements) {
		elements.push_back(value ? value->clone() : nullptr);
	}
}

JSONArray& JSONArray::operator=(const JSONArray& other) {
	if (this == &other) {
		return *this;
	}

	std::vector<std::unique_ptr<JSONValue>> copied_elements;
	copied_elements.reserve(other.elements.size());
	for (const auto& value : other.elements) {
		copied_elements.push_back(value ? value->clone() : nullptr);
	}

	elements = std::move(copied_elements);
	return *this;
}

size_t JSONArray::size() const {
	return elements.size();
}

const JSONValue * JSONArray::at(const size_t index) const {
	return elements.at(index).get();
}

void JSONArray::add(std::unique_ptr<JSONValue> value) {
	elements.push_back(std::move(value));
}

void JSONArray::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONArray::accept(JSONPrintVisitor& visitor) const {
	visitor.visit(*this);
}

std::unique_ptr<JSONValue> JSONArray::clone() const {
	return std::make_unique<JSONArray>(*this);
}
