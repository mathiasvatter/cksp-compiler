//
// Created by Mathias Vatter on 01.04.25.
//

#pragma once

#include "../ASTVisitor.h"

/**
 * Stores restrictive and thread unsafe ksp commands
 * Provides functions to check if a function is allowed in the current context
 */
class BuiltinRestrictionValidator final : public ASTVisitor {
public:
	/// stores for every restricted builtin function the callbacks where they are allowed
	/// purge_group can only be used in on ui_control, on ui_controls and on persistence_changed
	/// wait, wait_ticks, wait_async, stop_wait can not be used in on init
	/// save_array, save_array_str, load_array, load_array_str can be used in on init, on persistence_changed, on ui_control and on pgs_changed
	///
	inline static const std::unordered_map<std::string, std::unordered_set<std::string>> m_restricted_functions = {
		// purge_group: Nur in "ui_control", "ui_controls" und "persistence_changed" erlaubt
		{ "purge_group", { "ui_control", "ui_controls", "persistence_changed" } },
		// save_array und verwandte Funktionen: Erlaubt in "init", "persistence_changed", "ui_control" und "pgs_changed"
		{ "save_array",     { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "save_array_str", { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "load_array",     { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "load_array_str", { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "fs_get_filename", {"ui_control", "ui_controls"}},
		{ "set_snapshot_type", {"init"}},
		{ "set_map_editor_event_color", {"init"}},
		{ "watch_var", {"init"}},
		{ "watch_array_idx", {"init"}},
		{ "disable_logging", {"init"}},
		{ "set_note_controller", ([]() -> std::unordered_set<std::string> {
		 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
		 allowed.erase("init");
		 return allowed;
		})() },
		{ "set_poly_at", ([]() -> std::unordered_set<std::string> {
		 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
		 allowed.erase("init");
		 return allowed;
		})() },
	};

	inline static const std::unordered_map<std::string, std::unordered_set<std::string>> m_restricted_variables = {
	// EVENT_NOTE only allowed in on note, on release and on midi_in
		{ "EVENT_NOTE", { "note", "release", "midi_in" } },
		{ "EVENT_ID",     {"note", "release", "midi_in"} },
	};

	inline static const std::unordered_map<std::string, std::unordered_set<std::string>> m_thread_unsafe_functions = {
		// wait-Funktionen: Alle CALLBACKS außer "init"
	{ "wait", ([]() -> std::unordered_set<std::string> {
		 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
		 allowed.erase("init");
		 return allowed;
	   })() },
	{ "wait_ticks", ([]() -> std::unordered_set<std::string> {
		 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
		 allowed.erase("init");
		 return allowed;
	   })() },
	{ "wait_async", ([]() -> std::unordered_set<std::string> {
		 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
		 allowed.erase("init");
		 return allowed;
	   })() },
	{ "stop_wait", ([]() -> std::unordered_set<std::string> {
		 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
		 allowed.erase("init");
		 return allowed;
	   })() }
	};

	static bool check_function_callability(const NodeFunctionCall& node, NodeCallback* callback) {
		if (node.kind != NodeFunctionCall::Builtin) return true;
		if (!callback) return true;
		const auto definition = node.get_definition();
		if (!definition) {
			auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
			error.m_message = "Unable to find builtin function definition for <" + node.function->name + ">.";
			error.exit();
		}
		// check if called in restricted callback
		if (definition->is_restricted || !definition->is_thread_safe) {
			const auto callback_name = StringUtils::remove(callback->begin_callback, "on ");
			const auto allowed_callbacks = get_allowed_callbacks(node.function->name);
			if (!allowed_callbacks.contains(callback_name)) {
				auto error = get_raw_compile_error(ErrorType::SyntaxError, node);
				error.m_message = "Found restricted function. <" + node.function->name + "> can not be used in <" + callback->begin_callback + "> callback.";
				error.m_expected = "Allowed Callbacks are: \n";
				for (const auto& allowed_callback : allowed_callbacks) {
					error.m_expected += "<" + allowed_callback + ">, ";
				}
				error.m_expected.erase(error.m_expected.size() - 2);
				error.m_got = "<" + callback->begin_callback + ">";
				error.exit();
				return false;
			}
		}
		return true;
	}

	/// sets internal flags for builtin functions and their thread safety and restrictions
	/// fills in the allowed callbacks where this function can be used
	static void write_builtin_function_restrictions(NodeFunctionDefinition& node) {
		// check if restricted
		if (m_restricted_functions.contains(node.header->name)) {
			node.is_restricted = true;
		}
		// check if thread unsafe
		if (m_thread_unsafe_functions.contains(node.header->name)) {
			node.is_thread_safe = false;
		}
	}

	static void write_builtin_variable_restrictions(NodeDataStructure& node) {
		if (m_restricted_variables.contains(node.name)) {
			node.is_restricted = true;
		}
	}

	static std::unordered_set<std::string> get_allowed_callbacks(const std::string& func_name) {
		std::unordered_set<std::string> allowed;
		if (const auto it = m_restricted_functions.find(func_name); it != m_restricted_functions.end()) {
			allowed.insert(it->second.begin(), it->second.end());
		}
		if (const auto it = m_thread_unsafe_functions.find(func_name); it != m_thread_unsafe_functions.end()) {
			allowed.insert(it->second.begin(), it->second.end());
		}
		return allowed;
	}

};