//
// Created by Mathias Vatter on 06.07.26.
//

#pragma once

#include <string>
#include <unordered_map>
#include <vector>


enum class OptimizationLevel {
	Unset,
	None,       // No optimization
	Simple,     // Simple optimizations only
	Standard,   // Default optimization level
	Aggressive  // Maximal optimization
};
inline const std::unordered_map<std::string, OptimizationLevel> optimization_level_map = {
	{ "unset",      OptimizationLevel::Unset      },
	{ "none",       OptimizationLevel::None      },
	{ "simple",     OptimizationLevel::Simple    },
	{ "standard",   OptimizationLevel::Standard  },
	{ "aggressive", OptimizationLevel::Aggressive}
};

enum class ParameterPassing {
	Unset,
	ByValue,    // Pass by value
	ByReference // Pass by reference
};
inline const std::unordered_map<std::string, ParameterPassing> parameter_passing_map = {
	{ "unset",    ParameterPassing::Unset     },
	{ "value",    ParameterPassing::ByValue     },
	{ "reference", ParameterPassing::ByReference }
};


// Enum für Debugging-Modi
enum class DebugMode {
	Unset,
	Off,    // Debugging ausgeschaltet
	Basic,  // Grundlegendes Debugging
	Full    // Volles Debugging
};

struct CompilerConfig {
	std::optional<std::string> input_filename{};
	std::vector<std::string> outputs{};
	std::optional<std::string> standard_output_file{};
	OptimizationLevel optimization_level = OptimizationLevel::Unset;
	DebugMode debug_mode = DebugMode::Unset;
	ParameterPassing parameter_passing = ParameterPassing::Unset;
	std::optional<bool> combine_callbacks;
	std::optional<int> max_callback_depth;

	/// constructor with default values
	CompilerConfig() = default;
	/// only copies values if not empty / unset in other
	CompilerConfig& operator=(const CompilerConfig& other) {
		if (other.input_filename.has_value())
			input_filename = other.input_filename;

		if (!other.outputs.empty())
			outputs = other.outputs;

		if (other.standard_output_file.has_value())
			standard_output_file = other.standard_output_file;

		if (other.optimization_level != OptimizationLevel::Unset)
			optimization_level = other.optimization_level;

		if (other.debug_mode != DebugMode::Unset)
			debug_mode = other.debug_mode;

		if (other.parameter_passing != ParameterPassing::Unset)
			parameter_passing = other.parameter_passing;

		if (other.combine_callbacks.has_value())
			combine_callbacks = other.combine_callbacks;

		if (other.max_callback_depth.has_value())
			max_callback_depth = other.max_callback_depth;

		return *this;
	}

	void set_defaults() {
		input_filename = "";
		outputs = {};
		standard_output_file = "out.txt";
		optimization_level = OptimizationLevel::Standard;
		debug_mode = DebugMode::Off;
		parameter_passing = ParameterPassing::ByValue;
		combine_callbacks = false;
		max_callback_depth = 1000;
	}

};

/// combines two configs, where cli options have precedence over pragma options
static std::unique_ptr<CompilerConfig> combine_configs(const std::unique_ptr<CompilerConfig>& cli, const std::unique_ptr<CompilerConfig>& pragma) {
	auto out = std::make_unique<CompilerConfig>();
	out->set_defaults();

	// merge in
	if (pragma) *out = *pragma; // base
	if (cli)    *out = *cli;    // CLI overrides

	if (out->outputs.empty()) {
		out->outputs.push_back(out->standard_output_file.value());
	}
	return out;
}
