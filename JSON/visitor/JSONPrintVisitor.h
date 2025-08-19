//
// Created by Mathias Vatter on 09.05.25.
//

#pragma once

#include <iomanip>

#include "JSONVisitor.h"
#include <ostream>
#include <sstream>

class JSONPrintVisitor final : public JSONVisitor {
	std::stringstream out{};
	int indent_level = 0;

	void indent() {
		for (int i = 0; i<indent_level; ++i) {
			out << "  ";
		}
	}

public:
	explicit JSONPrintVisitor() = default;

	[[nodiscard]] std::string get_string(JSONValue& node) {
		out.str("");
		out.clear();
		node.accept(*this);
		return out.str();
	}

	void visit(JSONString& node) override {
		out << std::quoted(node.value);
	}

	void visit(JSONInt& node) override {
		out << node.value;
	}

	void visit(JSONFloat& node) override {
		out << node.value;
	}

	void visit(JSONBool& node) override {
		out << (node.value ? "true" : "false");
	}

	void visit(JSONNull& node) override {
		out << "null";
	}

	void visit(JSONObject& node) override {
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
			if (dynamic_cast<JSONValue*>(snd.get()) || dynamic_cast<JSONArray*>(snd.get())) {
				// Kein zusätzlicher Space vor Objekt/Array wenn pretty printing
			} else {
				out << " ";
			}
			snd->accept(*this);
			first = false;
		}

		indent_level--;
		if (!node.properties.empty()) {
			out << "\n";
			indent();
		}
		out << "}";
	}

	void visit(JSONArray& node) override {
		out << "[";
		if (!node.elements.empty()) out << "\n";
		indent_level++;

		bool first = true;
		for (const auto& elem : node.elements) {
			if (!first) {
				out << ",\n";
			}
			indent();
			elem->accept(*this);
			first = false;
		}

		indent_level--;
		if (!node.elements.empty()) {
			out << "\n";
			indent();
		}
		out << "]";
	}


};
