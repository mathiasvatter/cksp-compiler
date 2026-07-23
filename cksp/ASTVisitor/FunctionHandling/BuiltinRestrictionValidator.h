//
// Created by Mathias Vatter on 01.04.25.
//

#pragma once

#include "../ASTVisitor.h"
#include "../../Preprocessor/PreAST/PreASTConditions.h"

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
	inline static const std::unordered_map<std::string, std::unordered_set<std::string>> m_restricted_functions = {
		// purge_group: allowed only in "ui_control", "ui_controls", and "persistence_changed"
		{ "purge_group", { "ui_control", "ui_controls", "persistence_changed" } },
		// save_array and related functions: allowed in "init", "persistence_changed", "ui_control", and "pgs_changed"
		{ "save_array",     { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "save_array_str", { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "load_array",     { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "load_array_str", { "init", "persistence_changed", "ui_control", "pgs_changed", "ui_controls" } },
		{ "fs_get_filename", {"ui_control", "ui_controls"}},
		{ "set_snapshot_type", {"init"}},
		{ "set_map_editor_event_color", {"init"}},
		{ "watch_var", {"init"}},
		{ "set_num_user_zones", {"init"}},
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
		{ "exit", ([]() -> std::unordered_set<std::string> {
		 std::unordered_set<std::string> allowed(CALLBACKS.begin(), CALLBACKS.end());
		 allowed.erase("init");
		 return allowed;
		})() },
	};

	inline static const std::unordered_set<std::string> m_load_save_functions = {
		"load_array", "save_array", "load_array_str", "save_array_str"
	};

	static bool is_load_save_function(const std::string& func_name) {
		return m_restricted_functions.contains(func_name);
	}

	inline static const std::unordered_map<std::string, std::unordered_set<std::string>> m_restricted_variables = {
	// EVENT_NOTE only allowed in on note, on release and on midi_in
		{ "EVENT_NOTE", { "note", "release", "midi_in" } },
		{ "EVENT_ID",     {"note", "release", "midi_in"} },
		{ "EVENT_VELOCITY", {"note", "release", "midi_in"} },
		{ "NI_SIGNAL_TYPE", {"listener"} },
		{ "CC_NUM", {"controller"} },
		{ "POLY_AT_NUM", {"poly_at"} },
		{"NI_MOUSE_EVENT_TYPE", {"ui_control", "ui_controls"} },
		{"NI_MOUSE_EVENT_TYPE_DRAG", {"ui_control", "ui_controls"} },
		{"NI_MOUSE_EVENT_TYPE_DROP", {"ui_control", "ui_controls"} },
		{"NI_MOUSE_EVENT_TYPE_LEFT_BUTTON_DOWN", {"ui_control", "ui_controls"} },
		{"NI_MOUSE_EVENT_TYPE_LEFT_BUTTON_UP", {"ui_control", "ui_controls"} },
		{"NI_MOUSE_OVER_CONTROL", {"ui_control", "ui_controls"} },
	};

	inline static const std::unordered_map<std::string, std::unordered_set<std::string>> m_thread_unsafe_functions = {
		// wait functions: all CALLBACKS except "init"
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

	static bool is_persistence_command(const std::string& func_name) {
		// built once from the existing PERSISTENCE_TOKENS map (values are vectors of command names)
		static const std::unordered_set<std::string> persistence_commands = [] {
			std::unordered_set<std::string> commands;
			for (const auto& [tok, names] : PERSISTENCE_TOKENS) {
				commands.insert(names.begin(), names.end());
			}
			return commands;
		}();
		return persistence_commands.contains(func_name);
	}

	static bool is_builtin_with_side_effects(const std::string& func_name) {
		return func_name == "message" || m_restricted_functions.contains(func_name) || m_thread_unsafe_functions.contains(func_name);
	}

	static bool check_variable_callability(const NodeReference& node, NodeCallback* callback) {
		if (!callback) return true;
		if (node.kind != NodeReference::Builtin) return true;
		const auto declaration = node.get_declaration();
		if (!declaration) {
			// will (probably with ui controls) always lead to errors
			return true;
			// auto error = make_diagnostic(ErrorType::InternalError, node);
			// error.message = "Unable to find builtin variable declaration for <" + node.name + ">.";
			// error.exit();
		}
		if (declaration->is_restricted) {
			const auto callback_name = StringUtils::remove(callback->begin_callback, "on ");
			const auto allowed_callbacks = get_allowed_callbacks(node);
			if (!allowed_callbacks.contains(callback_name)) {
				auto error = make_diagnostic(ErrorType::SyntaxError, node);
				error.message = "Found restricted variable. <" + node.name + "> can not be used in <" + callback->begin_callback + "> callback.";
				error.expected = "Allowed Callbacks are: \n";
				for (const auto& allowed_callback : allowed_callbacks) {
					error.expected += "<" + allowed_callback + ">, ";
				}
				error.expected.erase(error.expected.size() - 2);
				error.actual = "<" + callback->begin_callback + ">";
				error.exit();
				return false;
			}
		}
		if (PreASTConditions::is_builtin_condition(node.tok)) {
			auto func_header = node.is_direct_func_arg();
			bool throw_error = !func_header;
			if (!throw_error) {
				throw_error = !PREPROCESSOR_SYNTAX.contains(func_header->name);
			}
			if(throw_error) {
				auto error = ASTVisitor::make_diagnostic(ErrorType::SyntaxError, node);
				error.set_message("Found built-in preprocessor variable in incorrcet environment. "
				"<" + StringUtils::join(PreASTConditions::BUILTIN_CONDITIONS, ", ") + "> can only be used in <(RE)SET_CONDITION>");
				error.exit();
			}
		}
		return true;
	}

	static bool check_function_callability(const NodeFunctionCall& node, NodeCallback* callback) {
		if (node.kind != NodeFunctionCall::Builtin) return true;
		if (!callback) return true;
		const auto definition = node.get_definition();
		if (!definition) {
			auto error = make_diagnostic(ErrorType::InternalError, node);
			error.message = "Unable to find builtin function definition for <" + node.function->name + ">.";
			error.exit();
		}
		// check if called in restricted callback
		if (definition->is_restricted || !definition->is_thread_safe) {
			const auto callback_name = StringUtils::remove(callback->begin_callback, "on ");
			const auto allowed_callbacks = get_allowed_callbacks(*node.function);
			if (!allowed_callbacks.contains(callback_name)) {
				auto error = make_diagnostic(ErrorType::SyntaxError, node);
				error.message = "Found restricted function. <" + node.function->name + "> can not be used in <" + callback->begin_callback + "> callback.";
				error.expected = "Allowed Callbacks are: \n";
				for (const auto& allowed_callback : allowed_callbacks) {
					error.expected += "<" + allowed_callback + ">, ";
				}
				error.expected.erase(error.expected.size() - 2);
				error.actual = "<" + callback->begin_callback + ">";
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

	static std::unordered_set<std::string> get_allowed_callbacks(const NodeFunctionHeaderRef& header) {
		const auto func_name = header.name;
		std::unordered_set<std::string> allowed;
		if (const auto it = m_restricted_functions.find(func_name); it != m_restricted_functions.end()) {
			allowed.insert(it->second.begin(), it->second.end());
		}
		if (const auto it = m_thread_unsafe_functions.find(func_name); it != m_thread_unsafe_functions.end()) {
			allowed.insert(it->second.begin(), it->second.end());
		}
		return allowed;
	}

	static std::unordered_set<std::string> get_allowed_callbacks(const NodeReference& node) {
		const auto var_name = node.name;
		std::unordered_set<std::string> allowed;
		if (const auto it = m_restricted_variables.find(var_name); it != m_restricted_variables.end()) {
			allowed.insert(it->second.begin(), it->second.end());
		}
		return allowed;
	}

};
