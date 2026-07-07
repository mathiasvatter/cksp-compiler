//
// Created by Mathias Vatter on 09.05.25.
//

#pragma once

#include <iomanip>

#include "JSONVisitor.h"
#include <ostream>
#include <sstream>

#include "../../utils/StringUtils.h"

class JSONPrintVisitor final : public JSONVisitor {
	std::stringstream out{};
	int indent_level = 0;

	void indent() {
		for (int i = 0; i<indent_level; ++i) {
			out << "  ";
		}
	}

	void write(const JSONString& node) {
		out << std::quoted(StringUtils::escape_json_string(node.value));
	}

	void write(const JSONInt& node) {
		out << node.value;
	}

	void write(const JSONFloat& node) {
		out << node.value;
	}

	void write(const JSONBool& node) {
		out << (node.value ? "true" : "false");
	}

	void write(const JSONNull& node) {
		out << "null";
	}

	void write(const JSONObject& node) {
		out << "{";
		if (!node.properties.empty()) out << "\n";
		indent_level++;

		bool first = true;
		for (const auto&[fst, snd] : node.properties) {
			if (!first) {
				out << ",\n";
			}
			indent();
			out << std::quoted(fst) << ": ";
			snd ? static_cast<const JSONValue&>(*snd).accept(*this) : write(JSONNull{});
			first = false;
		}

		indent_level--;
		if (!node.properties.empty()) {
			out << "\n";
			indent();
		}
		out << "}";
	}

	void write(const JSONArray& node) {
		out << "[";
		if (!node.elements.empty()) out << "\n";
		indent_level++;

		bool first = true;
		for (const auto& elem : node.elements) {
			if (!first) {
				out << ",\n";
			}
			indent();
			elem ? static_cast<const JSONValue&>(*elem).accept(*this) : write(JSONNull{});
			first = false;
		}

		indent_level--;
		if (!node.elements.empty()) {
			out << "\n";
			indent();
		}
		out << "]";
	}

public:
	explicit JSONPrintVisitor() = default;

	[[nodiscard]] std::string get_string(const JSONValue& node) {
		out.str("");
		out.clear();
		node.accept(*this);
		return out.str();
	}

	[[nodiscard]] std::string get_string(JSONValue& node) {
		return get_string(static_cast<const JSONValue&>(node));
	}

	void visit(JSONString& node) override {
		write(node);
	}

	void visit(const JSONString& node) {
		write(node);
	}

	void visit(JSONInt& node) override {
		write(node);
	}

	void visit(const JSONInt& node) {
		write(node);
	}

	void visit(JSONFloat& node) override {
		write(node);
	}

	void visit(const JSONFloat& node) {
		write(node);
	}

	void visit(JSONBool& node) override {
		write(node);
	}

	void visit(const JSONBool& node) {
		write(node);
	}

	void visit(JSONNull& node) override {
		write(node);
	}

	void visit(const JSONNull& node) {
		write(node);
	}

	void visit(JSONObject& node) override {
		write(node);
	}

	void visit(const JSONObject& node) {
		write(node);
	}

	void visit(JSONArray& node) override {
		write(node);
	}

	void visit(const JSONArray& node) {
		write(node);
	}

};
