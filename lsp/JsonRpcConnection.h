//
// Created by Mathias Vatter on 06.07.26.
//

#pragma once
#include <exception>
#include <iomanip>
#include <iostream>
#include <istream>
#include <mutex>
#include <ostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "JsonRpcMessage.h"
#include "../JSON/parser/JSONParser.h"
#include "../utils/StringUtils.h"

class JsonRpcConnection {
	std::istream& m_input;
	std::ostream& m_output;
	mutable std::mutex m_output_mutex;

	void write_json(const JSONValue& message) const {
		const std::string body = message.get_string();
		std::lock_guard lock(m_output_mutex);
		m_output << "Content-Length: " << body.size() << "\r\n"
				 << "\r\n"
				 << body;
		m_output.flush();
	}

public:
	JsonRpcConnection(std::istream& input, std::ostream& output) : m_input(input), m_output(output) {}

	[[nodiscard]] std::optional<JsonRpcMessage> read_message() const;

	void send_response(const JSONValue& id, const JSONValue& result) const;
	void send_error_response(const JSONValue& id, int code, const std::string& message) const;
	void send_notification(const std::string& method, const JSONValue& params) const;

};

inline std::optional<JsonRpcMessage> JsonRpcConnection::read_message() const {
	std::string line;
	std::size_t content_length = 0;

	// get content length of rpc message
	while (std::getline(m_input, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (line.empty()) break;

		constexpr std::string_view prefix = "Content-Length:";
		if (StringUtils::starts_with(line, prefix)) {
			const std::string value = line.substr(prefix.length());
			content_length = (std::size_t)std::stoul(value);
		}
	}

	if (content_length == 0 || !m_input.good()) {
		return std::nullopt;
	}

	std::string body(content_length, '\0');
	m_input.read(body.data(), body.size());

	if (m_input.gcount() != body.size()) {
		return std::nullopt;
	}

	try {
		return JsonRpcMessage(JSONParser::parse(std::move(body), "lsp"));
	} catch (const std::exception& e) {
		std::cerr << "Failed to parse JSON-RPC message: " << e.what() << std::endl;
		return std::nullopt;
	}
}

inline void JsonRpcConnection::send_response(const JSONValue& id, const JSONValue& result) const {
	JSONObject response;
	response.add("jsonrpc", std::make_unique<JSONString>("2.0"));
	response.add("id", id.clone());
	response.add("result", result.clone());
	write_json(response);
}

inline void JsonRpcConnection::send_error_response(const JSONValue& id, const int code, const std::string& message) const {
	auto error = std::make_unique<JSONObject>();
	error->add("code", std::make_unique<JSONInt>(code));
	error->add("message", std::make_unique<JSONString>(message));

	JSONObject response;
	response.add("jsonrpc", std::make_unique<JSONString>("2.0"));
	response.add("id", id.clone());
	response.add("error", std::move(error));
	write_json(response);
}

inline void JsonRpcConnection::send_notification(const std::string& method, const JSONValue& params) const {
	JSONObject notification;
	notification.add("jsonrpc", std::make_unique<JSONString>("2.0"));
	notification.add("method", std::make_unique<JSONString>(method));
	notification.add("params", params.clone());
	write_json(notification);
}
