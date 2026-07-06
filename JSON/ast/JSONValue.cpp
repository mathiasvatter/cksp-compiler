//
// Created by Mathias Vatter on 09.05.25.
//

#include "JSONValue.h"

#include "../visitor/JSONPrintVisitor.h"
#include "../visitor/JSONVisitor.h"

std::string JSONValue::get_string() {
	JSONPrintVisitor visitor;
	return visitor.get_string(*this);
}

void JSONString::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONInt::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONFloat::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONBool::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONNull::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONObject::add(const std::string &key, std::unique_ptr<JSONValue> value) {
	auto it = properties.find(key);
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

const JSONObject * JSONObject::get_object(const std::string &key) const {
	const auto it = properties.find(key);
	if (it != properties.end()) {
		return dynamic_cast<JSONObject*>(it->second.get());
	}
	return nullptr;
}

const JSONArray * JSONObject::get_array(const std::string &key) const {
	const auto it = properties.find(key);
	if (it != properties.end()) {
		return dynamic_cast<JSONArray*>(it->second.get());
	}
	return nullptr;
}

bool JSONObject::contains(const std::string &key) const {
	return properties.contains(key);
}

void JSONObject::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONArray::add(std::unique_ptr<JSONValue> value) {
	elements.push_back(std::move(value));
}

void JSONArray::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}
