//
// Created by Mathias Vatter on 25.01.24.
//

#pragma once
#include <string>
#include <utility>
#include <vector>
#include <filesystem>
#include <unordered_map>


enum class CmdOptions {
    Help,
    Version,
    Output,
    Compression,
	Optimization,
	CallbackCombining,
	ParameterPassing
};

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

		return *this;
	}

	void set_defaults() {
		input_filename = "";
		outputs = {};
		standard_output_file = std::filesystem::path(
				std::filesystem::path(input_filename.value()).parent_path() / "out.txt").string();
		optimization_level = OptimizationLevel::Standard;
		debug_mode = DebugMode::Off;
		parameter_passing = ParameterPassing::ByValue;
		combine_callbacks = false;
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


class CommandLineOptions {
public:
    CommandLineOptions(int argc, char* argv[]);
    ~CommandLineOptions() = default;

	[[nodiscard]] std::unique_ptr<CompilerConfig> get_compiler_config();

private:
	std::unique_ptr<CompilerConfig> m_compiler_config = nullptr;
	std::vector<std::tuple<std::string, std::string, std::string, CmdOptions, std::string>> m_option_tuples = {
            {"h", "help", "", CmdOptions::Help, "Display usage information"},
			{"o", "output", "<file>", CmdOptions::Output, "Write output to <file>. May be specified multiple times. If omitted, default is <input_dir>/out.txt."},
            {"v", "version", "", CmdOptions::Version, "Display version number"},
			{"O", "optimize", "<level>", CmdOptions::Optimization, "Set optimization level: none, simple, standard, aggressive"},
			{"", "-O0", "", CmdOptions::Optimization, "Equivalent to --optimize=none"},
			{"", "-O1", "", CmdOptions::Optimization, "Equivalent to --optimize=simple"},
			{"", "-O2", "", CmdOptions::Optimization, "Equivalent to --optimize=standard"},
			{"", "-O3", "", CmdOptions::Optimization, "Equivalent to --optimize=aggressive"},
			{"c", "combine-callbacks", "", CmdOptions::CallbackCombining, "Combine duplicate callbacks into one (enable)"},
			{"",  "no-combine-callbacks", "", CmdOptions::CallbackCombining, "Disable duplicate callback combining"},
			{"P", "pass-by", "<value|reference>", CmdOptions::ParameterPassing, "Force the parameter passing method for all function parameters. The default is 'value'. To imitate SublimeKSP behavior, use 'reference'."},
    };

	[[nodiscard]] std::string get_help_option() const;
};


