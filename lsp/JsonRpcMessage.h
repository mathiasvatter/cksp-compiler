//
// Created by Mathias Vatter on 07.07.26.
//

#pragma once

#include "../JSON/ast/JSONValue.h"
#include <optional>

/**
 * Model of JSON-RPC 2.0 request and response objects
 * a request object has the following members:
 *	- jsonrpc -> always 2.0
 *	- method -> string containing the name of the method to be invoked
 *	- params -> structured value (may be omitted)
 *	- id -> identifier established by client, can be String, Number, NULL
 *	a response object:
 *	 - jsonrpc -> always 2.0
 *	 - result -> required on success, prohibited on error
 *	 - error -> required if there was an error
 *	 - id -> required, upon error it must be NULL
 */
class JsonRpcMessage {
	std::unique_ptr<JSONValue> m_value;
	const JSONObject* m_object = nullptr;

public:
	explicit JsonRpcMessage(std::unique_ptr<JSONValue> value) : m_value(std::move(value)) {
		m_object = m_value->as<JSONObject>();
	}

	bool is_valid() const {
		if (!m_object) return false;
		const auto* version = m_object->get<JSONString>("jsonrpc");
		return version && version->value == "2.0";
	}

	bool is_response() const {
		if (!is_valid() || !id() || method()) return false;
		return result() || error();
	}

	bool is_request() const {
		return is_valid() && id() && method();
	}

	bool is_notification() const {
		return is_valid() && method() && !id();
	}

	const JSONObject* params_object() const {
		return params()->as<JSONObject>();
	}

	// getter for the members
	const JSONString* method() const {
		if (!m_object) return nullptr;
		return m_object->get<JSONString>("method");
	}

	const JSONValue* id() const {
		if (!m_object) return nullptr;
		return m_object->get("id");
	}

	const JSONValue* params() const {
		if (!m_object) return nullptr;
		return m_object->get("params");
	}

	const JSONValue* result() const {
		if (!m_object) return nullptr;
		return m_object->get("result");
	}

	const JSONValue* error() const {
		if (!m_object) return nullptr;
		return m_object->get("error");
	}

};