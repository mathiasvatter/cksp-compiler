//
// Created by Mathias Vatter on 09.05.25.
//

#pragma once

#include <ranges>

#include "../ast/JSONValue.h"

class JSONVisitor {
public:
	virtual ~JSONVisitor() = default;
	virtual void visit(JSONValue& node) {

	}
	virtual void visit(JSONString& node) {

	}
	virtual void visit(JSONInt& node) {

	}
	virtual void visit(JSONFloat& node) {

	}
	virtual void visit(JSONBool& node) {

	}
	virtual void visit(JSONNull& node) {

	}
	virtual void visit(JSONObject& node) {
		for (const auto &val : node.properties | std::views::values) {
			val->accept(*this);
		}
	}
	virtual void visit(JSONArray& node) {
		for (const auto & el : node.elements) {
			el->accept(*this);
		}
	}

};