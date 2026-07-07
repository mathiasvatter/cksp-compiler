//
// Created by Mathias Vatter on 07.07.26.
//

#include "LanguageServer.h"

int LanguageServer::run() {
	while (auto message = m_connection.read_message()) {
		handle_message(*message);
		if (m_shutdown_requested) {
			break;
		}
	}
	return 0;
}

void LanguageServer::handle_message(const JsonRpcMessage& message) {
	if (message.is_request()) {
		handle_request(message);
		return;
	}
	if (message.is_notification()) {
		handle_notification(message);
		return;
	}
}

void LanguageServer::handle_request(const JsonRpcMessage& message) {
	const auto* method = message.method();
	if (!method) return;

	if (method->value == "initialize") {
		handle_initialize(message);
	} else if (method->value == "shutdown") {
		handle_shutdown(message);
	}
}

void LanguageServer::handle_notification(const JsonRpcMessage& message) {
	const auto* method = message.method();
	if (!method) return;

	if (method->value == "initialized") {
		m_initialized = true;
	} else if (method->value == "textDocument/didOpen") {
		handle_did_open(message);
	} else if (method->value == "textDocument/didChange") {
		handle_did_change(message);
	} else if (method->value == "textDocument/didClose") {
		handle_did_close(message);
	} else if (method->value == "exit") {
		m_shutdown_requested = true;
	}
}

void LanguageServer::handle_initialize(const JsonRpcMessage& message) {
	m_initialized = true;

	JSONObject sync;
	sync.add("openClose", std::make_unique<JSONBool>(true));
	sync.add("change", std::make_unique<JSONInt>(1));

	JSONObject capabilities;
	capabilities.add("textDocumentSync", std::make_unique<JSONObject>(sync));

	JSONObject server_info;
	server_info.add("name", std::make_unique<JSONString>("cksp-lsp"));
	server_info.add("version", std::make_unique<JSONString>("0.1.0"));

	JSONObject result;
	result.add("capabilities", std::make_unique<JSONObject>(capabilities));
	result.add("serverInfo", std::make_unique<JSONObject>(server_info));

	if (const auto* id = message.id()) {
		m_connection.send_response(*id, result);
	}
}

void LanguageServer::handle_shutdown(const JsonRpcMessage& message) {
	m_shutdown_requested = true;
	if (const auto* id = message.id()) {
		m_connection.send_response(*id, JSONNull{});
	}
}

void LanguageServer::handle_did_open(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = params->get<JSONObject>("textDocument");
	const auto* uri = text_document->get<JSONString>("uri");
	const auto* text = text_document->get<JSONString>("text");
	if (!uri || !text) return;

	const auto version = text_document->get_int("version").value_or(0);
	m_sources.update(source_from_uri(uri->value), text->value, version);
}

void LanguageServer::handle_did_change(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = params->get<JSONObject>("textDocument");
	const auto* uri = text_document->get<JSONString>("uri");
	if (!uri) return;

	const auto* changes_value = params ? params->get("contentChanges") : nullptr;
	const auto* changes = changes_value ? changes_value->as<JSONArray>() : nullptr;
	if (!changes || changes->size() == 0) return;

	const auto* last_change = changes->at(changes->size() - 1);
	const auto* change = last_change ? last_change->as<JSONObject>() : nullptr;
	const auto* text = change->get<JSONString>("text");
	if (!text) return;

	const auto version = text_document->get_int("version").value_or(0);
	m_sources.update(source_from_uri(uri->value), text->value, version);
}

void LanguageServer::handle_did_close(const JsonRpcMessage& message) {
	const auto* params = message.params() ? message.params()->as<JSONObject>() : nullptr;
	const auto* text_document = params->get<JSONObject>("textDocument");
	const auto* uri = text_document->get<JSONString>("uri");
	if (!uri) return;

	m_sources.close(source_from_uri(uri->value));
}
