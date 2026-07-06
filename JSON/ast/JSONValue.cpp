//
// Created by Mathias Vatter on 09.05.25.
//

#include "JSONValue.h"

#include "../visitor/JSONPrintVisitor.h"
#include "../visitor/JSONVisitor.h"

std::string JSONValue::get_string() {
	static JSONPrintVisitor visitor;
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

void JSONObject::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}

void JSONArray::add(std::unique_ptr<JSONValue> value) {
	elements.push_back(std::move(value));
}

void JSONArray::accept(JSONVisitor& visitor) {
	visitor.visit(*this);
}
